<?php

include('db.php');

if (!$authed)
    return;

$output = [];
$method = $_GET['method'];
$app = urlencode($_GET['app']);

$resp = $mysql->query('SELECT * FROM `apps` WHERE `name` = \'' . $app . '\'');

if ($resp->num_rows > 0)
    $app = $resp->fetch_all(MYSQLI_ASSOC)[0];
else
    return;

switch ($method) {
    case 'add':
    {
        $mysql->query('INSERT INTO `builds` (`customId`, `appId`) VALUES (' . urlencode($_GET['id']) . ', ' . $app['id'] . ')');
        break;
    }
}

header('Content-Type: application/json; charset=utf-8');
print_r(json_encode($output, JSON_PRETTY_PRINT));