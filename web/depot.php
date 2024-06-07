<?php

function guidv4($data = null)
{
    // Generate 16 bytes (128 bits) of random data or use the data passed into the function.
    $data = $data ?? random_bytes(16);
    assert(strlen($data) == 16);

    // Set version to 0100
    $data[6] = chr(ord($data[6]) & 0x0f | 0x40);
    // Set bits 6-7 to 10
    $data[8] = chr(ord($data[8]) & 0x3f | 0x80);

    // Output the 36 character UUID.
    return vsprintf('%s%s-%s-%s-%s-%s%s%s', str_split(bin2hex($data), 4));
}

include('db.php');

$depotName = urlencode($_GET['name']);

if (!empty($_POST)) {
    if ($authed) {
        $appName = urlencode($_GET['app']);
        $buildName = urlencode($_GET['build']);

        $app = $mysql->query('SELECT * FROM `apps` WHERE `name` = \'' . $appName . '\'')->fetch_all(MYSQLI_ASSOC)[0];
        $build = $mysql->query('SELECT * FROM `builds` WHERE `customId` = \'' . $buildName . '\' AND `appId` = ' . $app['id'])->fetch_all(MYSQLI_ASSOC)[0];
        $mysql->query('DELETE FROM `depots` WHERE `buildId` = ' . $build['id'] . ' AND `filename` = \'' . $depotName . '\'');
        $uuid = guidv4();
        $filename = $_SERVER['DOCUMENT_ROOT'] . '/apps/' . $app['name'] . '/' . $build['customId'] . '/depots/' . $depotName;
        if (!file_exists(dirname($filename)))
            mkdir(dirname($filename), 0777, true);
        if(file_exists($filename))
            unlink($filename);
        $file = base64_decode($_POST['depot']);
        file_put_contents($filename, $file);
        $mysql->query('INSERT INTO `depots` (`buildId`, `filename`, `uuid`, `sha`, `keyId`, `size`) VALUES (' . $build['id'] . ', \'' . $depotName . '\', \'' . $uuid . '\', \'' . bin2hex(hash('sha256', $file, true)) . '\', NULL, ' . strlen($file) . ')');
    }
} else {
    $depotUuid = urlencode($_GET['uuid']);
    $depot = $mysql->query('SELECT * FROM `depots` WHERE `filename` = \'' . $depotName . '\' AND `uuid` = \'' . $depotUuid . '\'');
    if ($depot->num_rows > 0) {
        $depot = $depot->fetch_all(MYSQLI_ASSOC)[0];
        $build = $mysql->query('SELECT * FROM `builds` WHERE `id` = ' . $depot['buildId'])->fetch_all(MYSQLI_ASSOC)[0];
        $app = $mysql->query('SELECT * FROM `apps` WHERE `id` = ' . $build['appId'])->fetch_all(MYSQLI_ASSOC)[0];

        $filename = $_SERVER['DOCUMENT_ROOT'] . '/apps/' . $app['name'] . '/' . $build['customId'] . '/depots/' . $depotName;

        if (file_exists($filename)) {
            header('Content-Type: application/octet-stream');
            header("Content-Transfer-Encoding: Binary");
            readfile($filename);
            return;
        }
    }
}

