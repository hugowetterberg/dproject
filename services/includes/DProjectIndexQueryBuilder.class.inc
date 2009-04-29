<?php
// $Id$

class DProjectIndexQueryBuilder extends QueryBuilder {
  public function __construct() {
    parent::__construct();
    $this->add_table('p', 'INNER JOIN {dproject} AS p ON  p.nid=n.nid', 'n', array(
      'short_name' => array(
        'condition' => self::CONDITION_MATCH, 
        'sortable' => TRUE,
      ),
      'description' => array(
        'condition' => self::CONDITION_NONE, 
        'sortable' => FALSE,
      ),
    ));
  }

  public function query($fields=array(), $parameters=array()) {
    $parameters['type'] = 'dproject';
    return parent::query($fields, $parameters);
  }
}