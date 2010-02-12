<?php
// $Id$

/**
 * @file
 *  DProjectPackager class
 */

/**
 * DProjectPackager that uses the datapoints module to consume
 * release history feeds from remote project servers.
 *
 * @package dpremote
 */
class DPRemotePackager extends DProjectPackager implements dProjectUpdateablePackager {
  const NAMESPACE = 'http://goodold.se/project/dproject/dpremote';

  /**
   * Gets the the type name of the packager
   *
   * @return void
   */
  public function getTypeName() {
    return 'remote';
  }

  public function getName() {
    return t('Remote project server');
  }

  /**
   * Gets the url to the update feed
   *
   * @return void
   */
  public function getUpdateFeedUrl() {
    $project = $this->getProject();
    return $this->configuration->project_site . '/' . $project->short_name . '/' . $this->configuration->core_compatability . '.x';
  }

  /**
   * Gets the description for the dpremote packager
   *
   * @return string
   *  A description
   */
  public function getDescription() {
    $known = DPRemotePackager::knownProjectServers();
    $out = '';
    if (isset($known[$this->configuration->project_site])) {
      $out .= '<strong>' . t('Server') . ':</strong> ' . $known[$this->configuration->project_site] . '<br/>';
    }
    $url = $this->getUpdateFeedUrl();
    $out .= '<strong>' . t('Url') . ':</strong> ' . l($url, $url) . '<br/>';
    $out .= '<strong>' . t('Release creation mode') . ':</strong> ' . $this->configuration->create_mode;
    return $out;
  }


  /**
   * Constructs a release creation form
   *
   * @param array &$form
   *  A form array
   * @return void
   */
  public function releaseCreationForm(&$form) {
    $sources = $this->getSources();
    $options = array(
      '' => t('-- Select a release'),
    );
    foreach ($sources as $pid => $def) {
      $options[$def['title']] = $this->getVertexOptions($pid);
    }

    $form['source'] = array(
      '#type' => 'select',
      '#title' => t('Source release'),
      '#options' => $options,
    );

    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Create'),
    );
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
    if (empty($form_state['values']['source'])) {
      form_set_error('source', t('You have to select a source for the release'));
    }
  }

  /**
   * Handles submission of the release creation form
   * and fills in the required
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
    $dp_source = datapoint_source($this->configuration->datapoint_source);
    $vertex = datapoint_vertex($dp_source, $values['source']);
    $datapoint = datapoint_by_id($vertex->pid);
    $data = unserialize($datapoint->data);

    // Get the api version from the datapoint data
    $api_version = intval($data['api_version']);

    // Set version so that we can get the version string
    $rel->setVersion($api_version, $vertex->major_version, $vertex->minor_version, $vertex->version_extra);

    // Set the packager for the release
    $rel->setPackager($this->getId());

    if ($vertex->release_metadata) {
      $entries = unserialize($vertex->release_metadata);
      foreach ($entries as $name => $e) {
        $m = $rel->getMetadataObject($name, $e->namespace);
        foreach (get_object_vars($e->data) as $key => $value) {
          $m->$key = $value;
        }
      }
    }

    // Get metadata objects and set some properties
    $metadata = $rel->getMetadataObject();
    $rmetadata = $rel->getMetadataObject('remote', DPRemotePackager::NAMESPACE);
    $rmetadata->source = array(
      array(
        'feed' => $dp_source['source configuration']['url'],
        'file' => $vertex->file_url,
      ),
    );

    // Decode release type terms
    $release_terms = unserialize($vertex->release_terms);
    $release_type = array();
    foreach ($release_terms as $term) {
      if ($term->name == 'Release type') {
        switch ($term->value) {
          case 'Bug fixes':
            $release_type[] = 'bug_fixes';
          break;
          case 'New features':
            $release_type[] = 'features';
          break;
          case 'Security update':
            $release_type[] = 'security';
          break;
        }
      }
    }
    $metadata->terms = $release_type;

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

    print var_export($rel, TRUE);
    print 'Source';
    var_dump($dp_source);
    print 'Point';
    var_dump($datapoint);
    print 'Point data';
    var_dump($data);
    print 'Vertex';
    var_dump($vertex); die;

    $rel->save();

    $rid = $rel->getId();
    $pid = $packager->getId();

    // Send the user to the confirmation page
    drupal_goto("project/{$project->short_name}/packagers/{$pid}/release/confirm/{$rid}");
  }

  public function configurationForm() {
    // TODO: Only one hard-coded source here now, known/discovered sources
    // should be added here too.
    $source_opts = array(
      '' => t('-- Select a server'),
    );
    foreach (DPRemotePackager::knownProjectServers() as $uri => $name) {
      $source_opts[$uri] = $name;
    }

    $form = array();

    $api_matches = array();
    $core_compat = array();
    preg_match('/^\d+/', DRUPAL_CORE_COMPATIBILITY, $api_matches);
    $core = $api_matches[0];
    for ($i=4; $i<$core+3; $i++) {
      $core_compat[$i] = $i . '.x';
    }

    $form['core_compatability'] = array(
      '#type' => 'select',
      '#title' => t('Core compatability'),
      '#options' => $core_compat,
      '#default_value' => $core,
    );

    $form['create_mode'] = array(
      '#type' => 'select',
      '#title' => t('Release creation mode'),
      '#options' => array(
        'manual' => t('Manual'),
        'automatic' => t('Automatic (mirror)'),
      ),
      '#default_value' => 'manual',
    );

    $form['project_site'] = array(
      '#type' => 'select',
      '#title' => t('Known project servers'),
      '#options' => $source_opts,
      '#description' => l(t('Manage known servers'), 'admin/settings/dpremote/known_servers'),
    );

    $form['manual_settings'] = array(
      '#type' => 'fieldset',
      '#title' => t('Use a new project server'),
      '#description' => t('Enter the name and base url to the project server if it\'s not already available in the list of known project servers'),
      // TODO: Find a way to support collapsible when using ahah!
  //    '#collapsible' => TRUE,
  //    '#collapsed' => TRUE,
      'project_site_name' => array(
        '#type' => 'textfield',
        '#title' => t('Name'),
      ),
      'project_site_base_url' => array(
        '#type' => 'textfield',
        '#title' => t('Base Url'),
        '#description' => t('The base url to project update feeds from this server. This is usually in the format !uri. When fetching update status the project short name and drupal core version is appended to this uri forming !uri!example', array(
          '!uri' => UPDATE_DEFAULT_URL,
          '!example' => '/project_name/' . DRUPAL_CORE_COMPATIBILITY,
        )),
      ),
    );

    return $form;
  }

  public function configurationFormValidate($values) {
    $using_known = !empty($values['project_site']);
    $using_custom = !empty($values['project_site_name']) && !empty($values['project_site_base_url']);
    $partial_custom = !$using_custom && (!empty($values['project_site_name']) || !empty($values['project_site_base_url']));

    if ($using_custom) {
      $known = DPRemotePackager::knownProjectServers();
      if (isset($known[$values['project_site_base_url']])) {
        form_set_error('project_site_base_url', t('The url that you\'re trying to register has already been added as a known server'));
      }
    }

    if (!$using_known && !$using_custom) {
      if ($partial_custom) {
        form_set_error(empty($values['project_site_name'])?'project_site_name':'project_site_base_url',
          t('You must provide both name and base url when using a new project server'));
      }
      else {
        form_set_error('project_site', t('You must choose a project server by selecting a known project server or entering information for a new one.'));
      }
    }
    else if ($using_known && ($using_custom || $partial_custom)) {
      $known = DPRemotePackager::knownProjectServers();
      form_set_error('project_site', t('You must choose either to use a known project server or enter information for a new one, don\'t do both.'));
      form_set_error($using_custom || empty($values['project_site_name'])?'project_site_name':'project_site_base_url',
        t('Clear name and base url fields if you want to use the known server !name. Deselect the known server!further if you want to use a new server.', array(
          '!name' => $known[$values['project_site']],
          '!further' => $partial_custom?t(' and fill in both name and base url'):'',
      )));
    }
  }

  public function configurationFormSubmit($values) {
    if (!empty($values['project_site'])) {
      // The packager is using one of the known servers
      $project_site = $values['project_site'];
    }
    else {
      // A server should be added to the list of known servers
      drupal_write_record('dpremote_servers', $v=array(
        'url'  => $values['project_site_base_url'],
        'name' => $values['project_site_name'],
      ));
      $project_site = $values['project_site_base_url'];
    }

    return array(
      'create_mode' => $values['create_mode'],
      'project_site' => $project_site,
      'core_compatability' => $values['core_compatability'],
    );
  }

  /**
   * Updates the datapoint source for the packager.
   *
   * @return void
   */
  public function update(&$status_out=NULL) {
    $sid = $this->configuration->datapoint_source;
    if (datapoint_update_source($sid)) {
      $this->setMessage('dpremote', 'feed-fetch', 'Fetched updates from !uri', array(
          '!uri' => $this->getUpdateFeedUrl(),
        ), WATCHDOG_INFO);
    }
    else {
      $this->setMessage('dpremote', 'feed-fetch', 'Failed to fetch updates from remote project server from the uri !uri', array(
          '!uri' => $this->getUpdateFeedUrl(),
        ), WATCHDOG_ERROR);
    }
  }

  // public abstract function package();
  // public abstract function configureRelease();

  public function created(&$status_out=Null) {
    $project = $this->getProject();
    $url = $this->getUpdateFeedUrl();

    $sid = datapoint_add_source(array(
      'type' => 'dpremote_project_releases',
      'name' => $project->short_name . ' ' . $this->configuration->core_compatability . '.x release history',
      'parser' => 'datapoint_project',
      'source configuration' => array(
        'url' => $url,
      ),
      'managed' => TRUE,
    ));

    // Save the updated configuration with the source id
    $this->configuration->datapoint_source = $sid;
    drupal_write_record('dproject_packager', $v=array(
      'pid' => $this->getId(),
      'configuration' => serialize($this->configuration),
    ), array('pid'));
  }

  public function deleted() {

  }

  /**
   * Gets sources with optional name and type matching.
   *
   * @param string $name
   *  Optional. The name source to get.
   *  Set to NULL if you want all the sources.
   * @param int $filter
   *  Optional. The filter you want to apply to the sources. One, or a
   *  combination of the DProjectPackager::SOURCES_* flags. Defaults to 0.
   * @return array
   *  A an array of zero or more sources. Or a single source if a name was
   *  specified. If both name and filter was set and the named source didn't
   *  match the filter, NULL will be returned.
   */
  public function getSources($name=NULL, $filter=0) {
    static $sources;

    if (!$sources) {
      $sources = array();
      $sid = $this->configuration->datapoint_source;

      $res = db_query("SELECT pid, name FROM {datapoint} WHERE sid=%d", array(
        ':sid' => $sid,
      ));
      while ($obj = db_fetch_object($res)) {
        $sources[$obj->pid] = array(
          'title' => $obj->name,
          'options' => array($this, 'getVertexOptions'),
          'type' => DProjectPackager::SOURCES_STATIC | DProjectPackager::SOURCES_EXPLICIT | DProjectPackager::SOURCES_TRACKABLE,
        );
      }
    }

    if ($name) {
      list($source) = $this->filterSources($sources[$name]);
      return $source;
    }
    return $this->filterSources($sources);
  }

  public function getVertexOptions($pid) {
    $options = array();

    $res = db_query_range("SELECT vid, valid_from, major_version, minor_version, version_extra
      FROM {datapoint_project_vertex}
      WHERE pid=%d
      ORDER BY valid_from DESC", array(
        ':pid' => $pid,
      ), 0, 10);
    while ($obj = db_fetch_object($res)) {
      $version = sprintf('%d.x-%d.%s',
        $this->configuration->core_compatability,
        $obj->major_version,
        $obj->minor_version===NULL ? 'x' : $obj->minor_version);
      if (!empty($obj->version_extra)) {
        $version .= '-' . $obj->version_extra;
      }
      $version .= ' ' . date('(Y-m-d H:i O)', $obj->valid_from);
      $options[$obj->vid] = $version;
    }

    return $options;
  }

  public static function setKnownProjectServer($url, $name) {
    $exists = db_result(db_query("SELECT COUNT(*) FROM {dpremote_servers} WHERE url='%s'", array(
      ':url' => $url,
    )));
    $values = array(
      'url' => $url,
      'name' => $name,
    );
    $update = $exists?array('url'):NULL;
    drupal_write_record('dpremote_servers', $values, $update);
  }

  public static function deleteKnownProjectServer($url) {
    db_query("DELETE FROM {dpremote_servers} WHERE url='%s'", array(
      ':url' => $url,
    ));
  }

  public static function knownProjectServers($server=NULL, $remove=FALSE) {
    static $known_servers;

    if (!$known_servers) {
      $known_servers = array(
        UPDATE_DEFAULT_URL => 'drupal.org',
      );
      $res = db_query("SELECT url, name FROM {dpremote_servers}");
      while ($s = db_fetch_object($res)) {
        $known_servers[$s->url] = $s->name;
      }
    }

    return $known_servers;
  }
}