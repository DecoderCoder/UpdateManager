<?php
include('db.php');

$output = [];

$method = $_GET['method'];

$output['accessGroups'] = [];

switch ($method) {
    case 'add':
    {
        if ($authed) {
            $name = urlencode($_GET['name']);
            $value = urlencode($_GET['value']);

            if ($mysql->query('SELECT * FROM `accessGroups` WHERE `name` = \'' . $name . '\' OR `value` = \'' . $value . '\'')->num_rows == 0) {
                $mysql->query('INSERT INTO `accessGroups` (`name`, `value`) VALUES (\''.$name.'\', \''.$value.'\')');
                $output['status'] = 'ok';
            } else {
                $output['status'] = 'already_exists';
            }
        }
        break;
    }
}

foreach ($mysql->query('SELECT * FROM `accessGroups`') as $row) {
    $group = [];
    $group['name'] = $row['name'];
    $group['value'] = $row['value'];
    $group["keys"] = [];
    foreach ($mysql->query('SELECT * FROM `accessKeys` WHERE `accessGroupId` = ' . $row['id']) as $keyRow) {
        $key = [];
        $key['name'] = $keyRow['name'];
        $key['value'] = $keyRow['value'];
        $group["keys"][] = $key;
    }
    $output['accessGroups'][] = $group;
}

header('Content-Type: application/json; charset=utf-8');
print_r(json_encode($output, JSON_PRETTY_PRINT));