<?php
// $Id$

/**
 * @file
 *  DProjectPackager class and packager interfaces
 */

/**
 * Interface that must be implemented but packagers that can be updated
 *
 * @package dproject
 */
interface dProjectUpdateablePackager {
  /**
   * Fetches updates from the packager source
   *
   * @param string $status_out
   * @return void
   */
  public function update(&$status_out=NULL);
}

/**
 * Base class for all project packagers
 *
 * @package dproject
 */
class DProjectPackager {
  const SOURCES_TRACKABLE = 1;
  const SOURCES_STATIC = 2;
  const SOURCES_EXPLICIT = 4;
  const SOURCES_DEFAULT = 6;

  private static $loadedPackagers = array();

  private $packagerTypeName = '';
  private $projectNode = NULL;

  protected $nid = 0;
  protected $pid = 0;
  protected $configuration;

  /**
   * Constructor for the packager
   *
   * @param object $state
   *  The state of the packager
   *
   * @return object
   */
  function __construct($obj=NULL) {
    if ($obj) {
      $this->nid = $obj->nid;
      $this->pid = $obj->pid;
      $this->packagerTypeName = $obj->packager;
      $this->configuration = $obj->configuration;
    }
    else {
      $this->configuration = new stdClass();
    }
  }

  /**
   * Gets the the type name of the packager
   *
   * @return void
   */
  public function getTypeName() {
    if (!empty($this->packagerTypeName)) {
      $this->packagerTypeName;
    }
    else {
      return 'none';
    }
  }

  /**
   * Callback for getting the name of the packager
   *
   * @return string
   *  A description
   */
  public function getName() {
    return t('Unimplemented packager');
  }

  /**
   * Callback for getting a description for the packager
   *
   * @return string
   *  A description
   */
  public function getDescription() {
    return t('This packager type, !packager, is not available on your server', array(
      '!packager' => $this->packagerTypeName,
    ));
  }

  /**
   * Gets the description of a release's source
   *
   * @param DProjectRelease $release
   *  The the release object
   * @return string
   *  The description of the source
   */
  public function getSourceDescription($release) {
    return t('Packaged using a packager (!packager) that\'s not available on your server', array(
      '!packager' => $this->packagerTypeName,
    ));
  }

  /**
   * Callback for generating a form fragment for configuring
   * a new packager.
   *
   * @return array
   *  A form fragment
   */
  public function configurationForm() {
    return array();
  }

  /**
   * Callback for turning the submitted values for the configuration form
   * into a configuration array
   *
   * @param string $values
   * @return array
   *  A configuration array
   */
  public function configurationFormSubmit($values) {
    return array();
  }

  /**
   * Callback for validated the submitted values for the configuration form
   *
   * @param string $values
   * @return void
   */
  public function configurationFormValidate($values) {
  }

  /**
   * Callback that creates a temporary directory with the
   * files to package for the release.
   *
   * @param DProjectRelease $release
   *  A release object
   * @return string
   *  A path to a directory containing the files to package
   */
  public function package($release) {
  }

  /**
   * Configures a release object using the supplied information
   *
   * @param DProjectRelease $release
   *  The release object to configure
   * @param string $reference
   *  The reference that was used (tag, branch, sha1 and so on)
   * @return void
   */
  public function configureRelease(&$release, $reference) {
  }

  /**
   * Performs initialization work when the packager has been created
   *
   * @param string $status_out
   *  Used to pass back status messages
   * @return void
   */
  public function created(&$status_out=NULL) {
  }

  /**
   * Gets the sources that are available to create releases
   * from.
   *
   * @param string $name
   *  Optional. The name of the source.
   * @param int $filter
   *  Optional. A source type filter; one or a combination of the
   *  DProjectPackager::SOURCES_(TRACKABLE|STATIC|EXPLICIT) constants.
   *  Defaults to zero (no filter).
   * @return array
   *  An array of sources or a specific source if a name was supplied.
   */
  public function getSources($name=NULL) {
    return array();
  }

  /**
   * Helper function for subclasses implementing getSources().
   * Filters an array of sources by type.
   *
   * @param array $sources
   *  An array of sources.
   * @param int $filter
   *  Optional. A source type filter; one or a combination of the
   *  DProjectPackager::SOURCES_(TRACKABLE|STATIC|EXPLICIT) constants.
   *  Defaults to zero (no filter).
   * @return array
   *  An array of sources or a specific source if a name was supplied.
   */
  protected function filterSources($sources, $filter=NULL) {
    // Return the sources untouched if no filter was specified
    if (!$filter || $filter == (self::SOURCES_STATIC | self::SOURCES_TRACKABLE)) {
      return $sources;
    }

    // Return a empty array if the filter neither allows TRACKABLE nor STATIC
    // sources.
    if (!($filter & (self::SOURCES_STATIC | self::SOURCES_TRACKABLE))) {
      return array();
    }

    // Check if sources must be explicit
    $only_explicit = $filter & self::SOURCES_EXPLICIT;
    if ($only_explicit) {
      $filter -= self::SOURCES_EXPLICIT;
    }

    $final = array();
    foreach ($sources as $source => $def) {
      if (($def['type'] & $filter) &&
        (!$only_explicit || self::SOURCES_EXPLICIT & $def['type'])) {
          $final[$source] = $def;
      }
    }
    return $final;
  }

  /**
   * Deletes a project packager and performs all the necessary cleanup
   *
   * @param int/stdClass $packager
   *  The id of the packager that should be deleted, or a packager object
   * @return void
   */
  public function delete() {
    db_query("DELETE FROM {dproject_packager} WHERE pid = %d", array(
      ':pid' => $this->getId(),
    ));
    db_query("DELETE FROM {dproject_packager_message} WHERE pid = %d", array(
      ':pid' => $this->getId(),
    ));
    $this->deleted();
  }

  /**
   * Cleanup action when deleting a packager
   * Deletes the local working directory
   *
   * @return void
   */
  public function deleted() {
    $dp = DProject::singleton();
    $wdir = $this->workingDirectory();
    if (file_exists(dirname($wdir))) {
      $dp->deleteDirectory($wdir);
    }
  }

  /**
   * Gets the id of the project node.
   *
   * @return int
   */
  public function getNid() {
    return $this->nid;
  }

  /**
   * Helper function so that the packager can work
   * with a project node singleton internally.
   *
   * @return stdClass
   */
  protected function getProject() {
    if (!$this->projectNode || $this->projectNode->nid!=$this->getNid()) {
      $this->projectNode = node_load($this->getNid());
    }
    return $this->projectNode;
  }

  /**
   * Helper function for getting values from the configuration object.
   *
   * @param string $name
   *  The name of the configuration attribute
   * @param mixed $default
   *  Optional. The default value that should be used if the value doesn't exist.
   * @return void
   */
  protected function conf($name, $default=NULL) {
    if (is_object($this->configuration) && isset($this->configuration->$name)) {
      return $this->configuration->$name;
    }
    return $default;
  }

  /**
   * Gets the id of the packager
   *
   * @return int
   */
  public function getId() {
    return $this->pid;
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
  public function workingDirectory($release=NULL) {
    if ($release) {
      // Temporary working directories are stored in the temporary directory
      return file_directory_temp() . '/dproject/' . $release->getId();
    }
    else {
      // Permanent working directories are stored in the project file directory
      $dp = DProject::singleton();
      return $dp->projectFileDirectory($this->getNid()) . '/' . $this->getId();
    }
  }

  /**
   * Sets a error for the packager
   *
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
  public function setMessage($module, $error, $message, $args=array(), $severity=WATCHDOG_NOTICE) {
    $message_type = '';
    switch ($severity) {
      case WATCHDOG_EMERG:
      case WATCHDOG_ALERT:
      case WATCHDOG_CRITICAL:
        db_query("UPDATE {dproject_packager} SET status=0 WHERE pid=%d", array(
          ':pid' => $packager_id,
        ));
      case WATCHDOG_ERROR:
        $message_type = 'error';
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

    // If we have a error name this should be logged to the dproject_packager_message
    // table. Anything below WATCHDOG_WARNING will result in any existing error being cleared.
    if (!empty($error)) {
      // Delete any old message of the same type
      db_query("DELETE FROM {dproject_packager_message} WHERE pid=%d AND module='%s' AND name='%s'", array(
        ':pid' => $this->getId(),
        ':module' => $module,
        ':name' => $error,
      ));

      // Add everything that's a warning or worse to the message log
      if ($severity <= WATCHDOG_WARNING) {
        // Add new message
        $data = array(
          'pid' => $this->getId(),
          'module' => $module,
          'name' => $error,
          'message' => $message,
          'message_arguments' => json_encode($args),
          'severity' => $severity,
        );
        drupal_write_record('dproject_packager_message', $data);
      }
    }
  }

  /**
   * Constructs a release creation form
   *
   * @param array &$form
   *  A form array
   * @param bool $tracking
   *  Optional. Set to TRUE if the form is for a tracking release.
   *  Defaults to FALSE.
   * @return void
   */
  public function releaseCreationForm(&$form, $tracking=FALSE) {
    $dp = DProject::singleton();
    $last_release = $dp->getLatestRelease($project);

    $sources = array(
      '' => t('--Select a source'),
    );

    if (!$tracking) {
      $release_type_filter = variable_get('dproject_valid_release_source_types', DProjectPackager::SOURCES_DEFAULT);
    }
    else {
      $release_type_filter = DProjectPackager::SOURCES_TRACKABLE;
      if (variable_get('dproject_require_explicit_trackable_sources', TRUE)) {
        $release_type_filter |= DProjectPackager::SOURCES_EXPLICIT;
      }
    }

    foreach ($this->getSources(NULL, $release_type_filter) as $name => $def) {
      $sources[$name] = t($def['title']);
    }

    $form['source'] = array(
      '#type' => 'fieldset',
      '#title' => t('Source'),
      'source_name' => array(
        '#type' => 'select',
        '#options' => $sources,
        '#ahah' => array(
          'path' => 'dproject/ahah/release_source_configuration/' . $this->getNid() . '/' . $this->getId(),
          'wrapper' => 'source-configuration',
        ),
      ),
      'release_source_configuration' => array(
        '#type' => 'markup',
        '#value' => '<div id="source-configuration"></div>',
      ),
    );

    $version = FALSE;
    if ($last_release) {
      $version = $last_release->getVersion();
    }

    $form['api_version'] = array(
      '#type' => 'textfield',
      '#title' => t('Api version'),
      '#default_value' => $version ? $version[0] : 6,
      '#required' => TRUE,
    );

    $form['major_version'] = array(
      '#type' => 'textfield',
      '#title' => t('Major version'),
      '#default_value' => $version ? $version[1] : 1,
      '#required' => TRUE,
    );

    if (!$tracking) {
      $form['minor_version'] = array(
        '#type' => 'textfield',
        '#title' => t('Minor version'),
        '#default_value' => $version ? $version[2] + 1 : 0,
        '#required' => TRUE,
      );

      $form['version_extra'] = array(
        '#type' => 'textfield',
        '#title' => t('Version extra'),
        '#default_value' => $version ? $version[3] : '',
      );

      $form['release_type'] = array(
        '#type' => 'checkboxes',
        '#title' => t('Release type'),
        '#options' => $dp->releaseTypes(TRUE),
        '#required' => TRUE,
      );
    }

    $status_options = array(
      'unsupported' => t('Unsupported'),
      'supported' => t('Supported'),
      'excl_supported' => t('Exclusively supported'),
      'recommended' => t('Recommended'),
      'excl_recommended' => t('Recommended and exclusively supported'),
    );

    $form['release_status'] = array(
      '#type' => 'select',
      '#title' => t('Release status'),
      '#options' => $status_options,
    );

    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Create'),
    );

    return $form;
  }

  /**
   * Validates the release creation form
   *
   * @param array $form
   *  The form array
   * @param array $form_state
   *  The form state
   * @return void
   */
  public function releaseCreationFormValidate($form, $form_state) {
    $values = $form_state['values'];

    // Validate the versions that always have to be integers
    $nv = array(
      'api_version' => t('Api version'),
      'major_version' => t('Major version'),
    );
    foreach ($nv as $field => $title) {
      if (!is_numeric($values[$field])) {
        form_set_error($field, t('The !title must be a number', array(
          '!title' => $title,
        )));
      }
    }

    if (!$form['#tracking_release']) {
      // Validate the minor version number
      if (!DProject::isInt($values['minor_version']) || $values['minor_version']<0) {
        if ($values['version_extra'] != 'dev') {
          form_set_error('version_extra', 'You must create a "dev" release if you don\'t provide a numeric minor version number equal to or greater than zero');
        }
        else if ($values['minor_version']!='x') {
          drupal_set_message('Warning! The non-numeric or negative minor version number for this dev-release is interpreted as unspecified and will be replaced by an x', 'warning');
        }
      }

      // Warn when creating a dev release with a specified minor version
      if ($values['version_extra'] == 'dev' && DProject::isInt($values['minor_version'])) {
        drupal_set_message('Warning! You are creating a dev release so the minor version number for this release will be set as unspecified and replaced by an x', 'warning');
      }

      // Validate the version extra format
      if (!empty($values['version_extra']) && !preg_match('/^[_a-z0-9]+$/', $values['version_extra'])) {
        form_set_error('version_extra', t('The version extra field can only contain lower-case letters, numbers and underscores'));
      }
    }

    if (empty($values['source_identifier'])) {
      form_set_error('source_name', t('You have to select a valid source for the release'));
    }
    else {
      $source = $this->getSources($values['source_name']);
      if (isset($source['resolve'])) {
        $res = call_user_func_array($source['resolve'], array($values['source_identifier']));
        if ($res === FALSE) {
          form_set_error('source_identifier', t('The selected source is not valid'));
        }
      }
    }
  }

  /**
   * Handles submission of the release creation form
   * and redirects to the confirmation page
   *
   * @param array $form
   *  The form array
   * @param array $form_state
   *  The form state
   * @return void
   */
  public function releaseCreationFormSubmit($form, $form_state) {
    $values = $form_state['values'];
    $project = node_load($this->getNid());
    $rel = new DProjectRelease($project);
    $metadata = $rel->getMetadataObject();
    $rel->setTracking($form['#tracking_release']);

    if (!$rel->isTracking()) {
      // Make changes in minor version number when necessary
      if ($values['version_extra'] == 'dev' || !DProject::isInt($values['minor_version']) || $values['minor_version']<0) {
        $values['minor_version'] = NULL;
      }

      // Set release type information
      $release_type = array();
      foreach ($values['release_type'] as $type) {
        if ($type) {
          $release_type[] = $type;
        }
      }
      $metadata->terms = $release_type;
    }
    else {
      $values['minor_version'] = NULL;
      // TODO: Give some thought to if we really want to force the tracking
      // release to being a dev release.
      $values['version_extra'] = 'dev';
    }

    // Set version so that we can get the version string
    $rel->setVersion($values['api_version'], $values['major_version'], $values['minor_version'], $values['version_extra']);

    // Set the packager for the release and store its configuration
    $rel->setPackager($this->getId());
    $this->configureRelease($rel, $values['source_identifier']);

    // Check if this new release is exclusively supported
    if (drupal_substr($values['release_status'], 0, 5)=='excl_') {
      $metadata->exclusively_supported = TRUE;
      $values['release_status'] = drupal_substr($values['release_status'], 5);
    }

    // Interpret the release status value
    $status = $rel->getStatus();
    switch ($values['release_status']) {
      case 'unsupported':
        $status = DProjectRelease::UNSUPPORTED;
      break;
      case 'supported':
        $status = DProjectRelease::SUPPORTED;
      break;
      case 'recommended':
        $status = DProjectRelease::RECOMMENDED;
      break;
    }

    // Set release state and save it
    $rel->setStatus($status);
    $rel->setPublished(FALSE);
    $rel->setTemporary(TRUE);
    $rel->save();

    $rid = $rel->getId();
    $pid = $this->getId();

    // Send the user to the confirmation page
    drupal_goto("project/{$project->short_name}/packagers/{$pid}/release/confirm/{$rid}");
  }

  /**
   * Helper function that returns a
   * packager instance from a result row
   * from the dproject_packager table
   *
   * If an array is passed to this function
   * it must contain the following values:
   * nid, pid, configuration and packager.
   *
   * @param mixed $res
   *  A query result object or an array with the needed values
   * @return mixed
   *  Returns a packager instance, or FALSE if no more rows are to be
   *  found in the result
   *
   * @throws Exception
   *  A exception is thrown if instantiation failed or if the
   *  result doesn't contain enough information for creating
   *  a packager instance.
   */
  private static function packagerFromResult($res) {
    static $mandatory = array('nid', 'pid', 'configuration', 'packager');
    $row = is_array($res) ? $res : db_fetch_array($res);
    $dp = DProject::singleton();

    if ($row) {
      // Check that the mandatory values are here
      foreach ($mandatory as $key) {
        if (!isset($row[$key])) {
          throw new Exception(t('Missing the value "!value". Could not create packager from the supplied result.', array(
            '!value' => $key,
          )));
        }
      }

      // Return a cached packager if possible
      if (isset(DProjectPackager::$loadedPackagers[$row['pid']])) {
        return clone DProjectPackager::$loadedPackagers[$row['pid']];
      }

      // Decode configuration json and get the packager declaration info
      $row['configuration'] = json_decode($row['configuration']);
      $ptype = $dp->getPackagerTypes($row['packager']);
      try {
        if ($ptype) {
          // The packager type was known, create a instance of the declared packager class
          $packager = $dp->newInstanceOf($ptype['class'], array((object)$row));
        }

        if (!$packager) {
          // The packager was unknown or it's class declaration could not be resolved
          // creating a packager from the base packager class
          $packager = $dp->newInstanceOf('DProjectPackager', array((object)$row));
        }

        // Cache the packager
        DProjectPackager::$loadedPackagers[$row['pid']] = $packager;

        return clone $packager;
      }
      catch (Exception $e) {
        $message = 'Could not create a packager instance of the type !type. Details: !message';
        $args = array(
          '!type' => $row['packager'],
          '!message' => $e->getMessage(),
        );

        // Replace the type argument with a string that shows the class too
        if ($ptype) {
          $args['!type'] = t('!type (class: !class)', array(
            '!type' => $row['packager'],
            '!class' => $ptype['class'],
          ));
        }

        // Log and show the error in a message
        watchdog('dproject', $message, $args, WATCHDOG_ERROR);
        drupal_set_message(t($message, $args), 'error');

        // Rethrow the exception
        throw $e;
      }
    }
    return FALSE;
  }

  /**
   * Gets a packager from a packager id
   *
   * @param int $pid
   *  The id of the packager
   * @return stdClass
   *  A packager object
   *
   * @throws Exception
   *  A exception is thrown if packager instantiation failed
   */
  public static function load($pid) {
    // Return a cached packager if possible
    if (isset(DProjectPackager::$loadedPackagers[$row['pid']])) {
      return clone DProjectPackager::$loadedPackagers[$row['pid']];
    }

    $res = db_query("SELECT pid, nid, packager, configuration FROM {dproject_packager} WHERE pid=%d", array(
      ':pid' => $pid,
    ));
    return DProjectPackager::packagerFromResult($res);
  }

  public static function newInstanceOf($packager_type) {
    $dp = DProject::singleton();
    $info = $dp->getPackagerTypes($packager_type);
    if ($info) {
      $i = $dp->newInstanceOf($info['class']);
      return $i;
    }
  }

  /**
   * Gets all the packagers for a project
   *
   * @param int/stdClass $project
   *  The nid of the project or the project node
   * @return array(stdClass)
   *  Returns a array of packager objects
   *
   * @throws Exception
   *  A exception is thrown if packager instantiation failed
   */
  public static function getProjectPackagers($project) {
    if (is_object($project)) {
      $project = $project->nid;
    }

    $res = db_query("SELECT nid, pid, packager, configuration FROM {dproject_packager} WHERE nid=%d", array(
      ':nid' => $project,
    ));
    $packagers = array();
    while ($packager = DProjectPackager::packagerFromResult($res)) {
      $packagers[] = $packager;
    }
    return $packagers;
  }
}