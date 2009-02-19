<?php

/**
 * Implementation of hook_form
 *
 * @return array The edit or create form for the node
 **/
function _dproject_form(&$node, $form_state) {
  $type = node_get_types('type', $node);

  if ($type->has_title) {
    $form['title'] = array(
      '#type' => 'textfield',
      '#title' => check_plain($type->title_label),
      '#required' => TRUE,
      '#default_value' => $node->title,
      '#weight' => -5,
    );
  }

  $form['short_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Short project name'),
    '#required' => TRUE,
    '#weight' => -4,
    '#default_value' => $node->short_name,
  );

  $form['description'] = array(
    '#type' => 'textarea',
    '#title' => t('Project description'),
    '#required' => TRUE,
    '#weight' => -3,
    '#default_value' => $node->description,
  );

  if ($type->has_body) {
    $form['body_field'] = node_body_field($node, $type->body_label, $type->min_word_count);
  }

  if (module_exists('content')) {
    $form['short_name']['#weight'] = content_extra_field_weight('dproject', 'short_name');
    $form['description']['#weight'] = content_extra_field_weight('dproject', 'description');
  }

  return $form;
}