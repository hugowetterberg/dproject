<?php
// $Id$

/**
 * Class that defines the dproject resource
 */
class DProjectResource {
  /**
   * Creates a project
   *
   * @param object $project ["data"]
   * @return object
   *
   * @Access(callback='_dprojectservice_resource_access', args={'create'}, appendArgs=true)
   */
  public static function create($project) {
    $node = (object)$project;
    $node->type = 'dproject';

    // Load the required includes for drupal_execute
    module_load_include('inc', 'node', 'node.pages');
    $nid = NULL;

    // Setup form_state
    $values = (array)$node;
    self::adaptFormStateValues($type, $values);
    $form_state = array();
    $form_state['values'] = $values;
    $form_state['values']['op'] = t('Save');
    $ret = drupal_execute('event_node_form', $form_state, $node);

    // Fetch $nid out of $form_state
    $nid = $form_state['nid'];

    // TODO: Send information about which fields failed
    if ($errors = form_get_errors()) {
      return services_error(implode("\n", $errors), 406);
    }

    return (object)array(
      'nid' => $nid,
      'uri' => services_resource_uri(array('dproject', $nid)),
    );
  }

  /**
   * Retrieves a project
   *
   * @param int $nid ["path","0"]
   *  The nid of the project to get
   * @return object
   *
   * @Access(callback='_dprojectservice_resource_access', args={'view'}, appendArgs=true)
   * @Model(class='ResourceFeedModel', implementation='EventResourceFeedModel')
   */
  public static function retrieve($nid) {
    $node = node_load($nid);
    $node->uri = services_resource_uri(array('dproject', $event->nid));
    return $node;
  }

  /**
   * Updates a project
   *
   * @param int $nid ["path","0"]
   *  The nid of the project to update
   * @param object $project ["data"]
   *  The project object
   * @return object
   *
   * @Access(callback='_dprojectservice_resource_access', args={'update'}, appendArgs=true)
   */
  public static function update($nid, $project) {
    $node = (object)$project;
    $node->nid = $nid;

    $old_node = node_load($node->nid);
    if ($old_node->nid) {
      // Load the required includes for drupal_execute
      module_load_include('inc', 'node', 'node.pages');

      // Setup form_state.
      $values = (array)$node;
      self::adaptFormStateValues($type, $values);
      $form_state = array();
      $form_state['values'] = $values;
      $form_state['values']['op'] = t('Save');
      $form_state['node'] = (array)$old_node;

      $ret = drupal_execute($type . '_node_form', $form_state, $old_node);
    }
    else {
      return services_error(t('Node not found'), 404);
    }

    return $node->nid;
  }

  /**
   * Deletes a project
   *
   * @param int $nid ["path","0"]
   *  The nid of the project to get
   * @return bool
   *
   * @Access(callback='_dprojectservice_resource_access', args={'delete'}, appendArgs=true)
   */
  public static function delete($nid) {
    return node_delete($nid);
  }

  /**
   * Retrieves a listing of projects
   *
   * @param int $page ["param","page"]
   * @param string $fields ["param","fields"]
   * @param array $params ["param"]
   * @return array
   *
   * @Access(callback='user_access', args={'access content'}, appendArgs=false)
   */
  public static function index($page=0, $fields=array(), $params=array()) {
    $builder = new DProjectIndexQueryBuilder();
    if ($params['__action']=='describe') {
      return $builder->describe();
    }

    if (is_string($fields)) {
      $fields = preg_split('/,\s?/', $fields);
    }

    if (!user_access('administer nodes') || !isset($parameters['status'])) {
      $parameters['status'] = 1;
    }
    list($sql, $params) = $builder->query($fields, $parameters);
    $res = db_query_range($sql, $params, $page*20, 20);

    $nodes = array();
    while ($node = db_fetch_object($res)) {
      $node->uri = services_resource_uri(array($type, $node->nid));
      $nodes[] = $node;
    }
    return $nodes;
  }
}