<?php if ($release->isTracking()): ?>
  <div class="message alert"><?php print t('This is a snapshot release, and it\'s not guaranteed to be stable'); ?></div>
<?php endif ?>

<div class="file">
  <?php print l(t('Download'), $release->getFilePath(), array('attributes' => array('class' => 'download'))) ?>
  <div class="checksum"><label>MD5:</label> <span class="md5"><?php print $release->getMd5(); ?></span></div>
</div>

<p><?php
  print t('Packaged using the @name packager.', array(
    '@name' => $packager->getName(),
  ));
?></p>

<?php print $packager->getSourceDescription($release); ?>