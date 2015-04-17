<?php
	//Start user session.
	session_start();
	include 'reconnect.php';

	//session Variables.
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

	//Get stockid and Format to alphanumeric.
	$stockid = preg_replace("/[^A-Za-z0-9 ]/", '', $_POST['stockid']);

	//Create user command.
	$command = "WSC|" . $_SESSION["transNum"] . "|QUOTE|" . $_SESSION["userid"] . "," . $stockid;
	//Post quote request into recent logs array.
	array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sending Command = " . $command);
	
	//Write commmand to server.
	fwrite($client, $command . "\r\n");
	//Read response from server.
	$response = fread($client,8192);
	
	//Separate message into array.
	$responseArray = explode(",", array_pop(explode("|", $response)));
	if (!strcmp($responseArray[0], "SUCC"))
	{
		//Post success response to logs array.
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Quote Price for " . $stockid . " = $" . $responseArray[1] . " . Quote valid for another " . (25 + floor(($responseArray[2])/1000) - time()) . " seconds.");
	}
	else
	{
		//Quote Failed, post error message.
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Quote Command Failed; " . $responseArray[1]);
	}
	
	//Close the connection.
	fclose($client);
	
	//Redirect back to dashboard.
	header('Location: dashboard.php'); 
?>

