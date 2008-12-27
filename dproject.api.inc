<?php
/**
 * Functions that can be used by other modules to interact with the dproject module
 *
 * @version $Id$
 **/

// Include the class definitions for dproject
require_once('dproject.inc');

/**
 * Gets a project node by the short name
 *
 * @param string $short_name
 *  The short name of the project to get
 * @return stdClass
 *  The project node
 */
function dproject_get_project_by_name($short_name) {
  $res = db_query("SELECT nid FROM {dproject} WHERE short_name = '%s'", $short_name);
  if ($o = db_fetch_object($res)) {
    return node_load($o->nid);
  }
}

/**
 * Gets information about the available packager types
 *
 * @param string $type_name
 *  Optional. Supply a name if you wan information about
 *  a single packager type.
 *
 * @return array
 *  An array of packager type definitions, or a single packager
 *  type definition if a name was supplied
 */
function dproject_get_packager_types($type_name=NULL) {
  static $packagers;

  if (!$packagers) {
    $packagers = array();
    $pmodules = module_implements('dproject_packager');
    foreach ($pmodules as $module) {
      $impl = call_user_func($module . '_dproject_packager');
      foreach ($impl as $name => $def) {
        $packagers[$name] = $def;
      }
    }
    drupal_alter('dproject_packager_types', $packagers);
  }

  if (is_string($type_name)) {
    return $packagers[$type_name];
  }

  return $packagers;
}

/**
 * Gets the description of a packager
 *
 * @param int/stdClass $packager
 *  A packager object or the id of the packager
 * @return string
 *  A html-fragment describing the packager
 */
function dproject_packager_description($packager) {
  $pp = dproject_get_packager_types();
  if (is_numeric($packager)) {
    $packager = dproject_get_project_packager($packager);
  }

  if (isset($pp[$packager->packager])) {
    if (isset($pp[$packager->packager]['packager description'])) {
      return call_user_func($pp[$packager->packager]['packager description'], $packager);
    }
  }
  else {
    return '<div class="packager-description packager-description-error">' . t('Error: missing packager type information') . '</div>';
  }
}

/**
 * Gets the description of a release's source
 *
 * @param DProjectRelease $release
 *  The the release object
 * @return string
 */
function dproject_release_source_description($release) {
  $pp = dproject_get_packager_types();
  $packager = dproject_get_project_packager($release->get_packager());

  $description = '';
  if (isset($pp[$packager->packager])) {
    if (isset($pp[$packager->packager]['release source description'])) {
      $description = call_user_func($pp[$packager->packager]['release source description'], $release);
    }
  }

  if (empty($description)){
    return '<div class="source-description source-description-error">' . t('Error: missing source information') . '</div>';
  }

  return $description;
}

/**
 * Gets a project packager from a packager id
 *
 * @param int $pid
 *  The id of the packager
 * @return stdClass
 *  A packager object
 */
function dproject_get_project_packager($pid) {
  $res = db_query("SELECT pid, nid, packager, configuration FROM {dproject_packager} WHERE pid=%d", array(
    ':pid' => $pid,
  ));
  if ($p = db_fetch_object($res)) {
    $p->configuration = json_decode($p->configuration);
    return $p;
  }
}

/**
 * Gets the project file directory
 *
 * @param int/stdClass $project
 *  The nid of the project or the project node
 * @return string
 *  The path to the project directory. No checks are made to determine
 *  if the directory exist, is writeable or can be created.
 */
function dproject_project_file_directory($project) {
  if (is_object($project)) {
    $project = $project->nid;
  }
  if ($project) {
    return file_directory_path() . '/dproject/' . $project;
  }
}

/**
 * Helper function to get paths for working directories
 *
 * @param DProjectRelease $release
 *  Optional. Provide a release object if this is a temporary working
 *  directory. This is normally used for storing files that are about to
 *  be packaged. Non-temporary working directories are primarily used
 *  for keeping a working directory for a version control system.
 * @return string
 *  The path to the working directory. No checks are made to determine
 *  if the directory exist, is writeable or can be created.
 */
function dproject_working_directory_path($packager, $release=NULL) {
  if ($release) {
    // Temporary working directories are stored in the temporary directory
    return file_directory_temp() . '/dproject/' . $release->get_id();
  }
  else {
    // Permanent working directories are stored in the project file directory
    return dproject_project_file_directory($packager->nid) . '/' . $packager->pid;
  }
}

/**
 * Gets all the packagers for a project
 *
 * @param int/stdClass $project
 *  The nid of the project or the project node
 * @return array(stdClass)
 *  Returns a array of packager objects
 */
function dproject_get_project_packagers($project) {
  if (is_object($project)) {
    $project = $project->nid;
  }

  $packagers = array();
  $res = db_query("SELECT nid, pid, packager, configuration FROM {dproject_packager} WHERE nid=%d", array(
    ':nid' => $project,
  ));
  while ($packager = db_fetch_object($res)) {
    $packager->configuration = json_decode($packager->configuration);
    $packagers[] = $packager;
  }
  return $packagers;
}

/**
 * Deletes a project packager and performs all the necessary cleanup
 *
 * @param int/stdClass $packager
 *  The id of the packager that should be deleted, or a packager object
 * @return void
 */
function dproject_delete_project_packager($packager) {
  if (is_numeric($packager)) {
    $packager = dproject_get_project_packager($packager);
  }

  if ($packager) {
    db_query("DELETE FROM {dproject_packager} WHERE pid = %d", array(
      ':pid' => $packager->pid,
    ));
    db_query("DELETE FROM {dproject_packager_message} WHERE pid = %d", array(
      ':pid' => $packager->pid,
    ));
    // Allow the packager to perform it's own cleanup
    $packagers = dproject_get_packager_types();
    if (isset($packagers[$packager->packager]) && isset($packagers[$packager->packager]['packager deleted'])) {
      $configuration = call_user_func($packagers[$packager->packager]['packager deleted'], $packager);
    }
  }
}

/**
 * Sets a error for the packager
 *
 * @param int $packager_id
 *  The id of the packager
 * @param string $module
 *  The name of the module that's responsible for the error
 * @param string $error
 *  The name of the error
 * @param string $message
 *  The untranslated (english) error message
 * @param string $args
 *  The arguments that should be used in the message
 *  @see t()
 * @param int $severity
 *  Using the same severity levels as watchdog, as defined
 *  in RFC 3164 http://www.faqs.org/rfcs/rfc3164.html
 *  @see watchdog()
 *  @see watchdog_severity_levels()
 * @return void
 * @author Hugo Wetterberg
 */
function dproject_set_packager_message($packager_id, $module, $error, $message, $args=array(), $severity=WATCHDOG_NOTICE) {
  $message_type = '';
  switch ($severity) {
    case WATCHDOG_EMERG:
    case WATCHDOG_ALERT:
    case WATCHDOG_CRITICAL:
    case WATCHDOG_ERROR:
      $message_type = 'error';
      db_query("UPDATE {dproject_packager} SET status=0 WHERE pid=%d", array(
        ':pid' => $packager_id,
      ));
    break;
    case WATCHDOG_WARNING:
      $message_type = 'warning';
    break;
    case WATCHDOG_NOTICE:
    case WATCHDOG_INFO:
    case WATCHDOG_DEBUG:
      $message_type = 'status';
    break;
  }

  // Log the message, and show a message if we have a message type
  watchdog('dproject', $message, $args, $severity);
  if (!empty($message_type)) {
    drupal_set_message(t($message, $args), $message_type);
  }

  // Delete any old message of the same type
  db_query("DELETE FROM {dproject_packager_message} WHERE pid=%d AND module='%s' AND name='%s'", array(
    ':pid' => $packager_id,
    ':module' => $module,
    ':name' => $error,
  ));
  // Add new message
  $data = array(
    'pid' => $packager_id,
    'module' => $module,
    'name' => $error,
    'message' => $message,
    'message_arguments' => json_encode($args),
    'severity' => $severity,
  );
  drupal_write_record('dproject_packager_message', $data);
}

/**
 * Gets the path to a projects packager page
 *
 * @param stdClass|int $project
 *  The project node or the nid of the project node
 * @return string
 *  The path to the packager page
 */
function dproject_project_packagers_path($project) {
  if (is_numeric($project)) {
    $project = node_load($project);
  }
  if (is_object($project)) {
    return 'project/' . $project->short_name . '/packagers';
  }
}

/**
 * Gets the latest release for a project
 *
 * @param stdClass|int $project
 *  The project node or the nid of the project node
 * @return string
 *  The release object for the latest release
 */
function dproject_get_latest_release($project) {
  if (is_object($project)) {
    $project = $project->nid;
  }

  if (is_int($project)) {
    $res = db_query("SELECT rid FROM {dproject_release}
      WHERE project_nid=%d
      ORDER BY api_version DESC, major_version DESC, minor_version DESC, creates DESC");
    if ($o = db_fetch_object($res)) {
      return DProjectRelease::load($o->rid);
    }
  }

  return FALSE;
}

/**
 * Recursively and completely deletes a directory
 *
 * @param string $dir
 *  The path to the directory
 * @return void
 */
function dproject_delete_directory($dir) {
  if (is_dir($dir)) {
    if($dh = opendir($dir)) {
      // Loop through the contents of the directory
      while (false !== ($obj = readdir($dh))) {
        $path = $dir . '/' . $obj;
        if($obj == '.' || $obj == '..') { // Skip non-content
          continue;
        }
        if (is_dir($path)) { // Recurse into directories
          dproject_delete_directory($path);
        }
        else { // Delete files immediately
          unlink($path);
        }
      }
      closedir($dh);
    }
    rmdir($dir);
  }
  else if (file_exists($dir)) {
    // We really want to delete whatever is on this path
    // kill it even if it's a file
    unlink($dir);
  }
}

/**
 * Returns the release types as a associative array with
 * the machine-readable name as key and the human readable
 * name as value
 *
 * @param bool $localize
 *  Set to true if you want the human-readable name to
 *  be localized. As the old project module uses a human-
 *  readable value for release type terms this defaults
 *  to FALSE.
 *
 * @return array
 */
function dproject_release_types($localize=FALSE) {
  if ($localize) {
    return array(
      'bug_fixes' => t('Bug fixes'),
      'features' => t('New features'),
      'security' => t('Security update'),
    );
  }
  else
  {
    return array(
      'bug_fixes' => 'Bug fixes',
      'features' => 'New features',
      'security' => 'Security update',
    );
  }
}