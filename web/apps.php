<?php
include('db.php');

$output = [];
$output['apps'] = [];

$method = $_GET['method'];
$name = urlencode($_GET['name']);
$accessGroupName = urlencode($_GET['accessGroup']);

if (!empty($method)) {
    if ($authed)
        switch ($method) {
            case 'remove':
            {
                $mysql->query('UPDATE `apps` SET `deleted` = 1 WHERE `name` = \'' . $name . '\'');
                break;
            }
            case 'add':
            {
                if ($resp = $mysql->query('SELECT * FROM `apps` WHERE `name` = \'' . $name . '\'')) {
                    if ($resp->num_rows != 0) {
                        if ($resp->fetch_all(MYSQLI_ASSOC)[0]['deleted']) {
                            $method2 = $_GET['method2'];
                            switch ($method2) {
                                case "restore":
                                {
                                    $mysql->query('UPDATE `apps` SET `deleted` = 0 WHERE `name` = \'' . $name . '\'');
                                    goto finish;
                                    break;
                                }
                                case "create":
                                {
                                    $appId = $mysql->query('SELECT * FROM `apps` WHERE `name` = \'' . $name . '\'')->fetch_all(MYSQLI_ASSOC)[0]['id'];
                                    foreach ($mysql->query('SELECT * FROM `builds` WHERE `appId` = ' . $appId) as $build) {
                                        $mysql->query('DELETE FROM `depots` WHERE `buildId` = ' . $build['id']);
                                    }
                                    $mysql->query('DELETE FROM `builds` WHERE `appId` = ' . $appId);
                                    $mysql->query('DELETE FROM `apps` WHERE `name` = \'' . $name . '\'');
                                    break;
                                }
                                default:
                                {
                                    $output = [];
                                    $output['status'] = 'has_deleted';
                                    goto finish;
                                    break;
                                }
                            }
                        } else {
                            $output = [];
                            $output['status'] = 'already_exists';
                            goto finish;
                        }
                        goto finish;
                    }
                }

                $accessGroup = 'null';
                if ($accessGroupName != "null") {
                    $resp = $mysql->query('SELECT * FROM `accessgroups` WHERE `value` = \'' . $accessGroupName . '\'');
                    if ($resp->num_rows > 0) {
                        $accessGroup = $resp->fetch_all(MYSQLI_ASSOC)[0]['id'];
                    }
                }
                $mysql->query('INSERT INTO `apps`(`name`, `accessGroupId`) VALUES (\'' . $name . '\', ' . $accessGroup . ')');
                break;
            }
        }
}

finish:

foreach ($mysql->query('SELECT * FROM apps') as $row) {
    if (!$row['deleted']) {
        $app = ['name' => $row['name']];
        $output['apps'][] = $app;
    }
}


header('Content-Type: application/json; charset=utf-8');
print_r(json_encode($output, JSON_PRETTY_PRINT));