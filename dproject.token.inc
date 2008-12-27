<?php
/**
 * Implementation of token hooks to provide token values for
 * dproject nodes
 *
 * @version $Id$
 **/

/**
 * Implementation of hook_token_values().
 */
function dproject_token_values($type, $object = NULL, $options = array()) {
  $values = array();
  switch ($type) {
    case 'node':
      $node = $object;
      if ($node->type == 'dproject') {
        $values['short-name'] = $object->short_name;
      }
    break;
  }
  return $values;
}

/**
 * Implementation of hook_token_list().
 */
function dproject_token_list($type = 'all') {
  if ($type == 'node' || $type == 'all') {
    $tokens['node']['short-name'] = t('Short project name');
    return $tokens;
  }
}