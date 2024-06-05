<?php

$hostLogin = "admin";
$hostPassword = "admin";

$authed = apache_request_headers()['Authorization'];
if (!empty($authed)) {
    $credentials = explode(':', base64_decode($authed));
    $login = urlencode($credentials[0]);
    $pass = urlencode($credentials[1]);

    if ($login == $hostLogin && $pass == $hostPassword) {
        $authed = true;
    }
}
if ($authed !== true)
    $authed = false;

$authed = true;

$mysql = mysqli_connect('127.0.0.1', 'root', '');
$mysql->select_db('db');
