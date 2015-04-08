<?php

// Start new user session.
session_start();
include 'reconnect.php';

//Set session variables.
$_SESSION["transServers"] = array("b140.seng.uvic.ca","b147.seng.uvic.ca","b141.seng.uvic.ca","b148.seng.uvic.ca","b139.seng.uvic.ca","b135.seng.uvic.ca");
$_SESSION["userid"] = $_POST['username'];
$_SESSION["pending"] = "No Buy/Sell Command Pending";
$_SESSION["serverErr"] = "WARNING - Not Connected to Transaction Server.";
$_SESSION["transNum"] = 0;
$_SESSION["timeEnd"] = null;

$_SESSION["recentarray"] = array(date("h:i:sa") . " : Attempting to select a transaction server...");

if (reconnect())
{
	array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Logged in successfully as " . $_SESSION["userid"]);
}
else
{
	array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Logged in despite server error as " . $_SESSION["userid"]);
}

//Implode log array into <br> separated string and redirect to dashboard.	
$_SESSION["recent"] = implode("<br>", array_slice($_SESSION["recentarray"], 0, 25) );
header('Location: dashboard.php');

?>
