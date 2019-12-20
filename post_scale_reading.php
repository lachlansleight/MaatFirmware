<?php

header("Content-Type: text/plain");

require __DIR__.'/vendor/autoload.php';

use Kreait\Firebase\Factory;

$factory = (new Factory)
    ->withServiceAccount('./alarm-clock-scale-firebase-adminsdk-4u344-67329e23f5.json')
    ->withDatabaseUri('https://alarm-clock-scale.firebaseio.com/');

   
$database = $factory->createDatabase();

$body = file_get_contents('php://input');
$t = time();
date_default_timezone_set("Australia/Melbourne");
$date_time = date("Y-m-d h:ia", $t);

$newPost = $database
    ->getReference('readings/' . $date_time)
    ->set($body);
    

?>
</body></html>