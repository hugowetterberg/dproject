<?php
/**
 * Filter by status
 */
class dproject_views_handler_filter_release_status extends views_handler_filter_in_operator {
  function get_value_options() {
    if (!isset($this->value_options)) {
      $this->value_title = t('Status');
      $types = node_get_types();
      foreach ($types as $type => $info) {
        $options[$type] = $info->name;
      }
      $this->value_options = array(
        DPROJECT_RELEASE_STATUS_RECOMMENDED => t('Recommended'),
        DPROJECT_RELEASE_STATUS_SUPPORTED => t('Supported'),
        DPROJECT_RELEASE_STATUS_UNSUPPORTED => t('Unsupported'),
      );
    }
  }
}