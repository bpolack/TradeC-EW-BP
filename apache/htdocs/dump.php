<?php
	// Start user session
	session_start();
	include 'reconnect.php';

	//session Variables
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
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Could not connect to a transaction server. Display Summary Failed.");
				header('Location: accountsummary.php');
				die(); 
			}
		}
		else
		{
			//Redirect back to dashboard.
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Could not connect to a transaction server. Display Summary Failed.");
			header('Location: dumplog.php');
			die(); 
		}
	}

	if (strcmp($_POST['cmd'], "dumpall") === 0)
	{
		//Get filename and Format to alphanumeric.
		$filename = preg_replace("/[^A-Za-z0-9 ]/", '', $_POST['filename']);
		//Create user command
		$command = "WSC|" . $_SESSION["transNum"] . "|DUMPLOG|" . "./" . $filename;

		//Post summary request into recent logs array
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sending Command = " . $command);

		//Write commmand to server
		fwrite($client, $command . "\r\n");
		//Read response from server
		$response = fread($client,8192);
	
		//Post server response to logs array
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Server Response - " . $response);
		//Separate message into array.
		$responseArray = explode("|", $response);
		if (!strcmp($responseArray[3], "SUCC"))
		{
			//Post server response to logs array
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : DUMPLOG Success. ");
		
		}
		else
		{
			//Post fail response to logs array
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : DUMPLOG FAIL. ");
		}
	}
	else if (strcmp($_POST['cmd'], "dumpme") === 0)
	{
		//Get filename and Format to alphanumeric.
		$filename = preg_replace("/[^A-Za-z0-9 ]/", '', $_POST['filename']);
		//Create user command
		$command = "WSC|" . $_SESSION["transNum"] . "|DUMPLOG|" . $_SESSION["userid"] . ",./" . $filename;

		//Post summary request into recent logs array
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sending Command = " . $command);

		//Write commmand to server
		fwrite($client, $command . "\r\n");
		//Read response from server
		$response = fread($client,8192);
	
		//Post server response to logs array
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Server Response - " . $response);
		//Separate message into array.
		$responseArray = explode("|", $response);
		if (!strcmp($responseArray[3], "SUCC\0"))
		{
			//Post server response to logs array
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : DUMPLOG Success. ");
		
		}
		else
		{
			//Post fail response to logs array
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : DUMPLOG FAIL. ");
		}
	}

	//Close the connection
	fclose($client);
	
	//Redirect back to accountsummary.php.
	header('Location: dumplog.php'); 
?>


