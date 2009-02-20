<?php
// $Id$

/**
 * @file
 *  Views integration for dproject releases
 */

/**
 * Field handler to present a link to the release.
 */
class dproject_views_handler_field_release_page_link extends dproject_views_handler_field_release_version {
  function construct() {
    parent::construct();
    $this->additional_fields['short_name'] = array(
      'table' => 'dproject',
      'field' => 'short_name',
    );
  }

  function option_definition() {
    $options = parent::option_definition();

    $options['text'] = array('default' => '', 'translatable' => TRUE);

    return $options;
  }

  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);
    $form['text'] = array(
      '#type' => 'textfield',
      '#title' => t('Text to display'),
      '#default_value' => $this->options['text'],
    );
  }

  function render($values) {
    $version = $this->get_readable_release_version($values);
    $text = !empty($this->options['text']) ? $this->options['text'] : $version;
    return l($text, 'project/' . $values->{$this->aliases['short_name']} . '/releases/' . $version);
  }
}