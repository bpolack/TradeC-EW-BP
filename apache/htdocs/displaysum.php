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
			header('Location: accountsummary.php');
			die(); 
		}
	}

	//Create user command
	$command = "WSC|" . $_SESSION["transNum"] . "|DISPLAY_SUMMARY|" . $_SESSION["userid"];

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
		if(strlen($response) < 5)
		{
			$_SESSION["acctSum"] = "No Account Information Found.";
			$_SESSION["triggers"] = "No Active Triggers";
			$_SESSION["transHistory"] = "No Transaction History";
		}
		else
		{
			//Separate response into each component and set session variables.
			//response format is AcctInfo|OwnedStocks|ActiveTriggers|LastTenTransactions
			$triggerArray = nl2br($responseArray[6]);
			$stockArray = nl2br($responseArray[5]);
			$_SESSION["acctSum"] = "Account Balance = $" . round($responseArray[4]/100, 2) . "<br><br>Stocks Owned : <br>" . $stockArray;
			$_SESSION["triggers"] = "Stock, Type, Amount, Trigger <br>" . $triggerArray;
			
			//Post server response to logs array
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : DISPLAY_SUMMARY Success. ");
		}
	}
	else
	{
		//Post fail response to logs array
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : DISPLAY_SUMMARY FAIL. ");
	}

	//Close the connection
	fclose($client);
	
	//Redirect back to accountsummary.php.
	header('Location: accountsummary.php'); 
?>


