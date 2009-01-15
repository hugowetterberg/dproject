<?php
/**
 * Functions for working with project feeds
 *
 * @version $Id$
 **/
 
if (!defined('XML_NS')) {
  define('XML_NS', 'http://www.w3.org/2000/xmlns/');
}

if (!defined('XML_DC_NS')) {
  define('XML_DC_NS', 'http://purl.org/dc/elements/1.1/');
}

/**
 * Generates a release feed for the given project and api
 *
 * @param int|stdClass $pid
 *  The id of the project or project node to generate feed for
 * @param int $api_version
 *  The drupal version to generate feed for
 * @return string
 *  The contents of the feed
 */
function _dproject_feed_generate($pid, $api_version) {
  if (is_object($pid)) {
    $node = $pid;
    $pid = $project->nid;
  }
  else {
    $node = node_load($pid);
  }

  $dom = new DomDocument('1.0');
  $dom->formatOutput = TRUE;

  $project = $dom->createElement('project');
  $project->setAttributeNS(XML_NS, 'xmlns:dc', XML_DC_NS);
  $project->setAttributeNS(XML_NS, 'xmlns:dproject', DPROJECT_NAMESPACE);
  $dom->appendChild($project);
  module_invoke_all('dproject_feed_project_added', $node, $project);

  $project->appendChild($dom->createElement('title', $node->title));
  $project->appendChild($dom->createElement('short_name', $node->short_name));
  $project->appendChild($dom->createElementNS(DPROJECT_NAMESPACE, 'dproject:nid', $node->nid));
  $author = user_load($node->uid);
  if ($author) {
    $project->appendChild($dom->createElementNS(XML_DC_NS, 'dc:creator', $author->name));
  }
  $project->appendChild($dom->createElement('api_version', $api_version . '.x'));

  $recommended = dproject_get_recommended_release($node, $api_version);
  if ($recommended) {
    $rv = $recommended->get_version();
    $project->appendChild($dom->createElement('recommended_major', $rv[1]));
  }

  $supported = dproject_get_supported_major_versions($node, $api_version);
  if ($supported) {
    $project->appendChild($dom->createElement('supported_major', join(',', $supported)));
    $project->appendChild($dom->createElement('default_major', min($supported)));
  }

  $project->appendChild($dom->createElement('project_status', $node->status?'published':'unpublished'));
  $project->appendChild($dom->createElement('link', url('node/' . $node->nid, array('absolute'=>TRUE))));

  $releases = $project->appendChild($dom->createElement('releases'));
  $rel_objects = dproject_get_releases($node, $api_version, 10);

  foreach($rel_objects as $rel) {
    $release = $releases->appendChild($dom->createElement('release'));
    $version = $rel->get_version();
    $release->appendChild($dom->createElement('name', $node->short_name . ' ' . $rel->version_string()));
    $release->appendChild($dom->createElement('version', $rel->version_string()));
    $release->appendChild($dom->createElement('version_major', $version[1]));
    if (is_numeric($version[2])) {
      $release->appendChild($dom->createElement('version_patch', $version[2]));
    }
    if (!empty($version[3])) {
      $release->appendChild($dom->createElement('version_extra', $version[3]));
    }
    // TODO: This release link doesn't point to anything. Implement it in the menu system.
    $release->appendChild($dom->createElement('release_link', url('node/' . $node->nid . '/release/' . $rel->version_string(), array('absolute'=>TRUE))));
    $release->appendChild($dom->createElement('download_link', url($rel->get_file_path(), array('absolute'=>TRUE))));
    $release->appendChild($dom->createElement('date', $rel->get_created()));
    $release->appendChild($dom->createElementNS(DPROJECT_NAMESPACE, 'iso_date', date('c', $rel->get_created())));
    $release->appendChild($dom->createElement('mdhash', $rel->get_md5()));

    $terms = $release->appendChild($dom->createElement('terms'));
    $metadata = $rel->get_metadata_object();
    $release_terms = array();
    $release_types = dproject_release_types(FALSE);
    foreach ($metadata->terms as $term) {
      if (isset($release_types[$term])) {
        _dproject_feed_add_term($dom, $terms, $release_types[$term]);
      }
      else {
        _dproject_feed_add_term($dom, $terms, $term, FALSE);
      }
    }

    $mdata_list = $rel->get_metadata_list();
    foreach ($mdata_list as $md) {
      // Register the metadata namespace if needed. Use the existing 
      // prefix if it's defined.
      $prefix = $dom->lookupPrefix($md->namespace);
      if ($prefix) {
        $md->name = $prefix;
      }
      else {
        $project->setAttributeNS(XML_NS, 'xmlns:' . $md->name, $md->namespace);
      }
      _dproject_feed_add_metadata($dom, $release, $md);
    }
  }

  print '<pre>' . htmlspecialchars($dom->saveXML()) . '</pre>';
  die;
}

function _dproject_feed_add_metadata($dom, $release, $md) {
  $metadata = $release->appendChild($dom->createElementNS($md->namespace, $md->name . ':metadata'));
  $metadata->appendChild($dom->createCDATASection(json_encode($md->data)));
}

function _dproject_feed_add_term($dom, $terms, $value, $release_type=TRUE) {
  if (!$release_type) {
    return $terms->appendChild($dom->createElementNS(DPROJECT_NAMESPACE, 'dproject:tag', $value));
  }

  $term = $terms->appendChild($dom->createElement('term'));
  $term->appendChild($dom->createElement('name', 'Release type'));
  $term->appendChild($dom->createElement('value', $value));
}