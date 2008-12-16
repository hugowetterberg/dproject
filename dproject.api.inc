<?php
// $Id$

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
 * @return array
 *  An array of packager type definitions
 */
function dproject_get_packager_types() {
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
  
  return $packagers;
}

/**
 * Gets the description of a packager
 *
 * @param int/stdClass $packager 
 *  A packager object or the id of the packager
 * @return string
 *  A html-fragment describing the packager
 * @author Hugo Wetterberg
 */
function dproject_packager_description($packager) {
  $pp = dproject_get_packager_types();
  if (is_numeric($packager)) {
    $packager = dproject_get_project_packager($packager);
  }
  
  if (isset($pp[$packager->packager])) {
    if (isset($pp[$packager->packager]['description callback'])) {
      return call_user_func($pp[$packager->packager]['description callback'], $packager);
    }
  }
  else {
    return '<div class="packager-description packager-description-error">' . t('Error: missing packager type information') . '</div>';
  }
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
    // Allow the packager to perform it's own cleanup
    $packagers = dproject_get_packager_types();
    if (isset($packagers[$packager]) && isset($packagers[$packager]['packager deleted'])) {
      $configuration = call_user_func($packagers[$packager]['packager deleted'], $p);
    }
  }
}