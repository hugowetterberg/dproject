<?php
// $Id$

/**
 * @file
 *  Parser functions that have been moved here to reduce the footprint
 *  of the module when it's not parsing release update feeds.
 */

/**
 * Datapoint parser for project update feeds
 *
 * @param string $data
 * @param array $source
 * @param DatapointResults $res
 * @return void
 */
function _datapoint_project_datapoint_parseresult($data, $source, &$res) {
  $doc = new DOMDocument();
  $doc->loadXML($data);
  $xpath = new DOMXPath($doc);
  $xpath->registerNamespace('dp', 'http://goodold.se/project/dproject');
  $xpath->registerNamespace('dc', 'http://purl.org/dc/elements/1.1/');
  $x = new DatapointProjectXpath($xpath);

  $project = $x->single('/project');
  if ($project) {
    $title = $x->value('title', $project);
    $short_name = $x->value('short_name', $project);
    $api_version = $x->value('api_version', $project);

    // Create the data point
    $id = $short_name . ' ' . $api_version;
    $point = $res->point($id);
    $point->setName($title . ' ' . $api_version);

    // Set "static" release feed information
    $point->data('title', $title);
    $point->data('short_name', $short_name);
    $point->data('api_version', $api_version);
    $point->data('recommended_major', $x->value('recommended_major', $project));
    $point->data('supported_majors', $x->value('supported_majors', $project));
    $point->data('default_major', $x->value('default_major', $project));
    $point->data('published', $x->value('published', $project));
    $point->data('link', $x->value('link', $project));

    // Get project terms
    $terms = $xpath->query('terms/term', $project);
    $point->data('terms', _datapoint_project_datapoint_parseterms($terms, $x));


    // Iterate through releases and create vertextes
    $releases = $xpath->query('releases/release', $project);
    foreach ($releases as $release) {
      $time = $x->value('date', $release);
      $vertex = new DatapointVertex($time);
      $vertex->value('major_version', $x->value('version_major', $release));
      $vertex->value('minor_version', $x->value('version_patch', $release));
      $vertex->value('version_extra', $x->value('version_extra', $release));
      $vertex->value('status', $x->value('status', $release));
      $vertex->value('release_url', $x->value('release_link', $release));
      $vertex->value('file_url', $x->value('download_link', $release));
      $vertex->value('md5', $x->value('mdhash', $release));

      $metadata = array();

      $metadata_elements = $release->getElementsByTagName('metadata');
      foreach ($metadata_elements as $meta) {
        $data = json_decode($meta->nodeValue, TRUE);
        _datapoint_project_add_metadata($metadata, $meta->prefix, $meta->namespaceURI, $data);
      }

      $cvs_tag = $x->value('tag', $release);
      if ($cvs_tag) {
        _datapoint_project_add_metadata($metadata, 'cvs', 'http://goodold.se/project/dproject/dpcvs', array(
          'tag' => $cvs_tag,
        ));
      }
      $vertex->value('release_metadata', json_encode($metadata));

      // Get release terms
      $terms = $xpath->query('terms/term', $release);
      $vertex->value('release_terms', json_encode(_datapoint_project_datapoint_parseterms($terms, $x)));

      //Add vertex
      $point->addVertex($vertex);
    }
  }
}

function _datapoint_project_add_metadata(&$metadata, $name, $namespace, $data) {
  // Find existing metadata entry
  $m = FALSE;
  foreach ($metadata as $mname => &$info) {
    if ($info['namespace']==$namespace) {
      $m = &$info;
    }
  }

  // If no metadata entry existed
  if (!$m) {
    // Find a unique name if the name is taken
    if (isset($metadata[$name])) {
      $i = 2;
      while (isset($metadata[$name . '_' . $i])) {
        $i++;
      }
      $name = $name . '_' . $i;
    }

    // Add metadata entry
    $metadata[$name] = array(
      'data' => array(),
      'namespace' => $namespace,
    );

    // Store reference
    $m = &$metadata[$name];
  }

  // Add the values to the metadata entry
  foreach ($data as $key => $value) {
    $m['data'][$key] = $value;
  }
}

function _datapoint_project_datapoint_parseterms($terms, $x) {
  $term_data = array();
  foreach ($terms as $term) {
    $term_data[] = array(
      'name' => $x->value('name', $term),
      'value' => $x->value('value', $term),
    );
  }
  return $term_data;
}

class DatapointProjectXpath {
  private $xpath;

  function __construct($xpath) {
    $this->xpath = $xpath;
  }

  function query($query, $context=NULL) {
    if ($context) {
      return $this->xpath->query($query, $context);
    }
    else {
      return $this->xpath->query($query);
    }
  }

  function single($query, $context=NULL) {
    $l = $this->query($query, $context);
    if ($l->length) {
      return $l->item(0);
    }
  }

  function value($query, $context=NULL) {
    $l = $this->query($query, $context);
    if ($l->length) {
      return $l->item(0)->nodeValue;
    }
  }
}