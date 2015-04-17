<?php

//Start user session.
session_start();
include 'reconnect.php';

//Session Variables.
$_SESSION["transNum"] += 1;

//Address of the transaction server.
$addr = gethostbyname($_SESSION["selectedServer"]);

//Create a TCP/IP socket.
$client = stream_socket_client("tcp://$addr:44422", $errno, $errorMessage, 15);
if ($client === false) 
{
    if(reconnect())
	{
		$addr = gethostbyname($_SESSION["selectedServer"]);
		$client = stream_socket_client("tcp://$addr:44422", $errno, $errorMessage, 15);	
		if ($client === false)
		{
			//Redirect back to dashboard.
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Could not connect to a transaction server. Command Failed.");
			header('Location: dashboard.php');
			die(); 
		}
	}
	else
	{
		//Redirect back to dashboard.
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Could not connect to a transaction server. Command Failed.");
		header('Location: dashboard.php');
		die(); 
	}
}

//Get amount and format to standard dollar value without symbols.
$amount = preg_replace("/[^0-9.]/", '', $_POST['amount']);
$amount = number_format($amount, 2, '.', '');

//Create user command.
$command = "WSC|" . $_SESSION["transNum"] . "|ADD|" . $_SESSION["userid"] . "," . $amount;
//Post add request into recent logs array.
array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sending Command = " . $command);

//Write commmand to server.
fwrite($client, $command . "\r\n");
//Read response from server.
$response = fread($client,8192);

array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Response received - " . $response);
//Separate message into array.
$responseArray = explode(",", array_pop(explode("|", $response)));
array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Array pop - " . $responseArray[0]);
if (!strcmp($responseArray[0], "SUCC\0"))
{
	//Post success response to logs array.
	array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Add Command Success. Added $" . $amount);
}
else
{
	//Add Failed, post error message.
	array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Add Command Failed; " . $responseArray[1]);
}

//Close the connection.
fclose($client);

//Redirect back to dashboard.
header('Location: dashboard.php'); 

?>
