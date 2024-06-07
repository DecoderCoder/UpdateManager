<?php

include('db.php');


$app = urlencode($_GET['appId']);
$resp = $mysql->query('SELECT * FROM `apps` WHERE `name` = \'' . $app . '\' AND `deleted` = 0');

if ($resp->num_rows == 0) {
    http_response_code(404);
    return;
}
$app = $resp->fetch_all(MYSQLI_ASSOC)[0];

$platform = urlencode($_GET['platform']);
$channel = urlencode($_GET['channel']);
$buildId = (int)$_GET['buildId'];
$build = null;

if ($buildId == 0) {
    $resp = $mysql->query('SELECT * FROM `builds` WHERE `appId` = ' . $app['id'] . ' AND id=(SELECT MAX(id) FROM `builds`)');
    if ($resp->num_rows != 0) {
        $build = $resp->fetch_all(MYSQLI_ASSOC)[0];
        $buildId = $build['customId'];
    }
}

$depots = $mysql->query('SELECT * FROM `depots` WHERE `buildId` = ' . $build['id']);
if ($depots->num_rows == 0)
    $depots = [];
$output = [];

$output['appId'] = $app['name'];
$output['platform'] = $platform;
$output['channel'] = $channel;
$output['buildId'] = $buildId;
$output['depots'] = [];

foreach ($depots as $file) {
    $depot = [];
    $depot['name'] = basename($file['filename']);
    $depot['size'] = $file['size'];
    $depot['url'] = '/depots/' . $file['uuid'] . '/' . basename($file['filename']).'.depot';
    $depot['mac'] = $file['sha']; // sha256 checksum of file
    $output['depots'][] = $depot;
}

if ($app['accessGroupId'] != null) {
    $accessGroup = $mysql->query('SELECT * FROM `accessGroups` WHERE `id` = ' . $app['accessGroupId'])->fetch_all(MYSQLI_ASSOC)[0];
    $output['keys']['accessGroup'] = $accessGroup['value'];
}


header('Content-Type: application/json; charset=utf-8');
print_r(json_encode($output, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));