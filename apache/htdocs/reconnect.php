<?php

//Start user session.
session_start();

function reconnect() 
{
    $numUsers = null;
	$currentServer = null;
	$_SESSION["selectedServer"] = null;

	//Command to find userid on a transaction server.
	$command = "USR|" . $_SESSION["userid"];
	
	array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Finding a valid transaction server.");
	//Iterate through Transaction Server list and find one associated with userid or one with least number of users.
	foreach ($_SESSION["transServers"] as &$value) 
	{
    	//Address of the transaction server.
		$addr = gethostbyname($value);
		//Create a TCP/IP socket.
		$client = stream_socket_client("tcp://$addr:44422", $errno, $errorMessage, 15);
		if ($client === false) 
		{
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Trans Server not responding -  " . $addr);
			//Could not connect (Server Down?).
			continue;
		}
		//Write commmand to server.
		fwrite($client, $command . "\r\n");
		//Read response from server.
		$response = fread($client,8192);
		//Separate message into array.
		$responseArray = explode("|", $response);
		
		if (!strcmp($responseArray[0], "HAVE"))
		{
			//Transaction Server has UserID.
			$_SESSION["selectedServer"] = $value;
			break;
		}
		else if (!strcmp($responseArray[0], "NOT"))
		{
			//Transaction Server does not have UserID, select if number of users is less than current.
			if ($numUsers > $responseArray[1] || $numUsers == null) 
			{
				$numUsers = $responseArray[1];
				$currentServer = $value;
			}
		}
	}
	
	//If UserID not found, set the selected server to the least busy in the list.
	if ($_SESSION["selectedServer"] == null && $currentServer != null)
	{
		$_SESSION["selectedServer"] = $currentServer;
	} 
	
	//Valid Transaction Server selected, success and redirect to cmd.
	if ($_SESSION["selectedServer"] != null)
	{
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Selected Transaction Server is " . $_SESSION["selectedServer"]);
		$_SESSION["serverErr"] = "Connected to Transaction Server " . $_SESSION["selectedServer"];
		return TRUE;
	}
	//Could not find a valid server.
	else
	{
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : WARNING - Could not connect to a valid transaction server.");
		$_SESSION["serverErr"] = "WARNING - Not Connected to Transaction Server.";
		return FALSE;
	}
	
}


?>
