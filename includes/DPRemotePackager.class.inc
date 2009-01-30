<?php

class DPRemotePackager extends DProjectPackager {
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
    $out .= '<strong>' . t('Url') . ':</strong> ' . l($url, $url);
    return $out;
  }
  
  // public function getSourceDescription() {
  //   
  // }
  
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
      $project_site = $values['project_site'];
    }
    else {
      $project_site = $values['project_site_base_url'];
    }

    return array(
      'project_site' => $project_site,
      'core_compatability' => $values['core_compatability'],
    );
  }
  
  // public abstract function update();
  // public abstract function package();
  // public abstract function configureRelease();
  
  public function created(&$status_out=Null) {
    $project = $this->getProject();
    $url = $this->getUpdateFeedUrl();
    
    datapoint_add_source(array(
      'type' => 'dpremote_project_releases',
      'name' => $project->short_name . ' ' . $this->configuration->core_compatability .'.x release history',
      'parser' => 'datapoint_project',
      'source configuration' => array(
        'url' => $url,
      ),
      'managed' => TRUE,
    ));
  }
  
  // public abstract function deleted();
  // public abstract function getSources();
  
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