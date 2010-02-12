<?php
// $Id$

/**
 * @file
 *  Contains the implementation of project administration interface
 */

/**
 * Implementation of hook_form().
 *
 * @return array The edit or create form for the node
 */
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

  return $form;
}

function _dproject_project_packagers($form_state, $node) {
  $dp = DProject::singleton();

  drupal_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l($node->title, 'node/' . $node->nid),
  ));
  drupal_set_title(t('!title packagers', array(
    '!title' => $node->title,
  )));

  $packagers = $dp->getPackagerTypes();
  $pack_options = array(
    '' => t('--Select a packager')
  );
  foreach ($packagers as $name => $def) {
    $pack_options[$name] = $def['name'];
  }

  $pp = DProjectPackager::getProjectPackagers($node->nid);
  $po = '<ul>';
  foreach ($pp as $p) {
    $po .= '<li><h3>' . $p->getName() . '</h3>';
    $po .= '<div class="packager-description">';
    $po .= $p->getDescription();
    $po .= '</div>';

    $packager_links = array(
      l(t('Create a release'), 'project/' . $node->short_name . '/packagers/' . $p->getId() . '/release', array(
        'attributes' => array(
          'class' => 'package',
        ),
      )),
      l(t('Create a tracking release'), 'project/' . $node->short_name . '/packagers/' . $p->getId() . '/tracking-release', array(
        'attributes' => array(
          'class' => 'package',
        ),
      )),
      l(t('Remove'), 'project/' . $node->short_name . '/packagers/' . $p->getId() . '/delete', array(
        'attributes' => array(
          'class' => 'delete',
        ),
      )),
    );

    // Add a update link if it's supported by the packager type
    if (in_array("dProjectUpdateablePackager", class_implements($p))) {
      $packager_links[] = l(t('Update'), 'project/' . $node->short_name . '/packagers/' . $p->getId() . '/update', array(
        'attributes' => array(
          'class' => 'update',
        ),
      ));
    }

    $po .= '<ul class="actions">';
    foreach ($packager_links as $link) {
      $po .= '<li>' . $link . '</li>';
    }
    $po .= '</ul>';

    $po .= '</li>';
  }
  $po .= '</ul>';

  $form = array(
    '#submit' => array('_dproject_project_packagers_submit'),
    '#validate' => array('_dproject_project_packagers_validate'),
    'packagers' => array(
      '#type' => 'markup',
      '#value' => $po,
    ),
    'project' => array(
      '#type' => 'value',
      '#value' => $node->nid,
    ),
    'packager' => array(
      '#type' => 'select',
      '#title' => t('Add a packager'),
      '#options' => $pack_options,
      '#ahah' => array(
        'path' => 'dproject/ahah/packager_configuration',
        'wrapper' => 'packager-configuration',
      ),
    ),
    'packager_configuration' => array(
      '#type' => 'markup',
      '#value' => '<div id="packager-configuration"></div>',
    ),
  );
  return $form;
}

function _dproject_project_packagers_validate($form, $form_state) {
  $values = $form_state['values'];
  $packager_type = $values['packager'];
  $packager = DProjectPackager::newInstanceOf($packager_type);
  $packager->configurationFormValidate($values);
}

function _dproject_project_packagers_submit($form, $form_state) {
  $values = $form_state['values'];
  $packager_type = $values['packager'];

  $packager = DProjectPackager::newInstanceOf($packager_type);
  $configuration = $packager->configurationFormSubmit($values);

  $values = array(
    'nid' => $values['project'],
    'packager' => $packager_type,
    'configuration' => serialize($configuration),
  );

  drupal_write_record('dproject_packager', $values);
  $pid = db_last_insert_id('dproject_packager', 'pid');
  $packager = DProjectPackager::load($pid);
  $packager->created();
}

function _dproject_create_project_release($form_state, $project, $packager, $tracking=FALSE) {
  drupal_set_title(t('Create a!tracking release of !name', array(
    '!tracking' => $tracking ? ' ' . t('tracking') : '',
    '!name' => $project->title,
  )));
  drupal_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l($project->title, 'node/' . $project->nid),
    l('Packagers', 'node/' . $project->nid . '/packagers'),
  ));

  $form = array(
    '#tracking_release' => $tracking,
    '#id' => 'dproject_create_project_release',
    '#validate' => array('_dproject_create_project_release_validate'),
    '#submit' => array('_dproject_create_project_release_submit'),
  );

  $form['pid'] = array(
    '#type' => 'value',
    '#value' => $packager->getId(),
  );

  $packager->releaseCreationForm($form, $tracking);
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
function _dproject_create_project_release_validate($form, $form_state) {
  $values = $form_state['values'];
  $packager = DProjectPackager::load($values['pid']);

  // Version validation
  // Construct version query argument array
  $v = array(
    ':project_nid' => $values['pid'],
    ':api_version' => $values['api_version'],
    ':major_version' => $values['major_version'],
    ':minor_version' => empty($values['minor_version']) ? 
      NULL : $values['minor_version'],
    ':version_extra' => empty($values['version_extra']) ? 
      '' : $values['version_extra'],
  );

  $exists = db_result(db_query("SELECT COUNT(project_nid) 
    FROM {dproject_release}
    WHERE temporary=1
      AND project_nid=%d 
      AND api_version=%d 
      AND major_version=%d
      AND minor_version=%d
      AND version_extra='%s'",
    $v));

  // Set error if we already have a release for this version
  if ($exists) {
    form_set_error('', t('There is already a release with this version number.'));
  }

  $packager->releaseCreationFormValidate($form, $form_state);
}

/**
 * Handles submission of the release creation form
 *
 * @param array $form
 *  The form array
 * @param array $form_state
 *  The form state
 * @return void
 */
function _dproject_create_project_release_submit($form, $form_state) {
  $packager = DProjectPackager::load($form_state['values']['pid']);
  $packager->releaseCreationFormSubmit($form, $form_state);
}

/**
 * Displays a release confirmation page
 *
 * @param array $form
 *  The form array
 * @param array $form_state
 *  The form state
 * @param string $packager
 *  The used packager
 * @param string $rid
 *  The if of the release
 * @return void
 */
function _dproject_confirm_project_release($form_state, $project, $packager, $rid) {
  drupal_add_css(drupal_get_path('module', 'dproject') . '/css/admin.css');

  $dp = DProject::singleton();
  $rel = DProjectRelease::load($rid);

  drupal_set_title(t('Confirm release settings for !name', array('!name' => $project->title)));

  $metadata = $rel->getMetadataObject();
  $version = $rel->getVersion();

  switch ($rel->getStatus()) {
    case DProjectRelease::UNSUPPORTED:
      $status = t('Unsupported');
    break;
    case DProjectRelease::SUPPORTED:
      $status = t('Supported');
    break;
    case DProjectRelease::RECOMMENDED:
      $status = t('Recommended');
    break;
  }

  $form = array(
    '#id' => 'dproject-confirm-release-configuration-form',
    '#submit' => array('_dproject_project_release_create'),
    'release' => array(
      '#type' => 'value',
      '#value' => $rel->getId(),
    ),
    'intro_text' => array(
      '#value' => '<p>' . t('Please confirm that the release version is correct and that the source is properly configured before creating the release') . '</p>',
      '#weight' => -10,
    ),
    'version' => array(
      '#type' => 'item',
      '#title' => t('Version'),
      '#value' => $rel->versionString(),
      '#weight' => -8,
    ),
    'release_status' => array(
      '#type' => 'item',
      '#title' => t('Release status'),
      '#value' => $status,
      '#weight' => -8,
    ),
    'packager_description' => array(
      '#type' => 'item',
      '#title' => t('Packager (@name)', array('@name' => $packager->getName())),
      '#value' => $packager->getDescription(),
      '#weight' => -6,
    ),
    'source_description' => array(
      '#type' => 'item',
      '#title' => t('Source description'),
      '#value' => $packager->getSourceDescription($rel),
      '#weight' => -5,
    ),
    // TODO: The cancel link should be rewired to a middle step that deletes the
    // temporary release entry, minimizing the cleanup work that cron has to do.
    'cancel_link' => array(
      '#type' => 'markup',
      '#value' => l(t('Cancel'), 'node/' . $project->nid),
    ),
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Create release'),
      '#weight' => 10,
    ),
  );

  if ($rel->isTracking()) {
    $form['tracking'] = array(
      '#type' => 'item',
      '#title' => t('Tracking'),
      '#value' => t('This release will track it\'s source, creating snapshot releases at regular intervals.'),
      '#weight' => -8,
    );
  }

  if (isset($metadata->terms)) {
    // Fill an array with the human readable names
    // of the selected release types
    $rt = array();
    $release_types = $dp->releaseTypes(TRUE);
    foreach ($metadata->terms as $term) {
      if (isset($release_types[$term])) {
        $rt[] = $release_types[$term];
      }
    }

    // Add release type description to the form
    $form['release_type'] = array(
      '#type' => 'item',
      '#title' => t('Release type'),
      '#value' => join($rt, '<br/>'),
      '#weight' => -8,
    );
  }

  if (isset($metadata->exclusively_supported) && $metadata->exclusively_supported) {
    $form['exclusive'] = array(
      '#type' => 'item',
      '#title' => t('Exclusivity'),
      '#value' => t('This release has been marked as exclusively supported. All releases for Drupal !api_version with a lower version number will therefore be marked as unsupported.', array(
        '!api_version' => $version[0],
      )),
      '#weight' => -7,
    );
  }

  return $form;
}

function _dproject_project_release_create($form, $form_state) {
  $dp = DProject::singleton();
  $release = DProjectRelease::load($form_state['values']['release']);

  $release->package();
  $release->setPublished(TRUE);
  $release->setTemporary(FALSE);
  $release->save();

  $metadata = $release->getMetadataObject();
  $v = $release->getVersion();
  // Set preceding releases with the same api-version as unsupported
  // if this release should be exclusively supported.
  if (isset($metadata->exclusively_supported) && $metadata->exclusively_supported) {
    // The reason that we get the id's of the releases that should be marked
    // as unsupported instead of directly updating the table is that we want to make all
    // changes through the api. This opens up for adding stuff like a audit trail later.
    $res = db_query("SELECT rid FROM {dproject_release}
      WHERE api_version=%d
      AND rid!=%d
      AND status!=%d", array(
        ':api' => $v[0],
        ':rid' => $release->getId(),
        ':status' => DProjectRelease::UNSUPPORTED,
    ));
    while ($urid = db_result($res)) {
      $dp->changeReleaseStatus($urid, DProjectRelease::UNSUPPORTED);
    }
  }
  // Make sure that only one release for this api version is marked as recommended.
  // If the release was marked as exclusively supported this has already been taken
  // care of.
  else if ($release->getStatus()==DProjectRelease::RECOMMENDED) {
    $res = db_query("SELECT rid FROM {dproject_release}
      WHERE api_version=%d
      AND rid!=%d
      AND status=%d", array(
        ':api' => $v[0],
        ':rid' => $release->getId(),
        ':status' => DProjectRelease::RECOMMENDED,
    ));
    while ($urid = db_result($res)) {
      $dp->changeReleaseStatus($urid, DProjectRelease::SUPPORTED);
    }
  }

  $node = $release->getProjectNode();
  drupal_goto('node/' . $node->nid);
}

function _dproject_ahah_release_source_configuration($packager) {
  $source = $_POST['source_name'];
  _dproject_ahah_helper('source/release_source_configuration', '_dproject_release_configuration_form', $source, $packager);
}

function _dproject_release_configuration_form($source, $packager, $configuration=array()) {
  $def = $packager->getSources($source);

  if (isset($def['options'])) {
    $options = call_user_func($def['options'], $source);
    return array(
      'source_identifier' => array(
        '#type' => 'select',
        '#options' => $options,
      ),
    );
  }
  return array();
}

function _dproject_update_project_packager($node, $packager) {
  if (in_array("dProjectUpdateablePackager", class_implements($packager))) {
    $packager->update();
  }
  drupal_goto('node/' . $node->nid . '/packagers');
}

function _dproject_delete_project_packager($form_state, $node, $packager) {
  $out .= '<p>' . t('Do you really want to delete the !type project packager for @project:', array(
    '!type' => $packager->getTypeName(),
    '@project' => $node->title,
  )) . '</p>';
  $out .= '<div class="packager-description">' . $packager->getDescription() . '</div>';

  return array(
    '#submit' => array('_dproject_delete_project_packager_confirmed'),
    'pid' => array(
      '#type' => 'value',
      '#value' => $packager->getId(),
    ),
    'confirm_text' => array(
      '#type' => 'markup',
      '#value' => $out,
    ),
    'cancel_link' => array(
      '#type' => 'markup',
      '#value' => l(t('No, cancel this action'), 'project/' . $node->short_name . '/packagers'),
    ),
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Yes, go ahead'),
    ),
  );
}

function _dproject_delete_project_packager_confirmed($form, $form_state) {
  $pid = $form_state['values']['pid'];
  $p = DProjectPackager::load($pid);
  if ($p) {
    $p->delete();
    $node = node_load($p->getNid());
    drupal_goto('project/' . $node->short_name . '/packagers');
  }
  else {
    drupal_not_found();
  }
}

function _dproject_ahah_packager_configuration() {
  $packager = DProjectPackager::newInstanceOf($_POST['packager']);
  _dproject_ahah_helper('packager_configuration', '_dproject_packager_configuration_form', $packager);
}

function _dproject_packager_configuration_form($packager, $configuration=array()) {
  $form = array();
  if ($packager) {
    $form['_conf'] = $packager->configurationForm();
  }
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add'),
  );
  return $form;
}

function _dproject_packager_configuration_submit($packager, $configuration=array()) {
  $packagers = dproject_get_packager_types();
  if (isset($packagers[$packager]) && isset($packagers[$packager]['configuration submit'])) {
    return call_user_func($packagers[$packager]['configuration submit']);
  }
}