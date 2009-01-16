<?php
/**
 * Field handler to present a link to the release.
 */
class dproject_views_handler_field_release_link extends views_handler_field {
  function construct() {
    parent::construct();
    $this->additional_fields['file_path'] = 'file_path';
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

  function query() {
    $this->ensure_my_table();
    $this->add_additional_fields();
  }

  function render($values) {
    $text = !empty($this->options['text']) ? $this->options['text'] : $values->{$this->aliases['file_path']};
    return l($text, $values->{$this->aliases['file_path']});
  }
}