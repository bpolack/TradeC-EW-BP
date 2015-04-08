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

/******************************************************
Set Buy Amount
*******************************************************/
	if (strcmp($_POST['cmd'], "setbuy") === 0)
	{
		//Check if buy/sell in progress.
		if ($_SESSION["buyPending"] != 1 && $_SESSION["sellPending"] != 1)
		{
			//Get stockid and Format to alphanumeric.
			$stockid = preg_replace("/[^A-Za-z0-9 ]/", '', $_POST['stockid']);
			//Get amount and format to standard dollar value without symbols.
			$amount = preg_replace("/[^0-9.]/", '', $_POST['amount']);
			$amount = number_format($amount, 2, '.', '');
	
			//Create user command.
			$command = "WSC|" . $_SESSION["transNum"] . "|SET_BUY_AMOUNT|" . $_SESSION["userid"] . "," . $stockid . "," . $amount;
		
			//Post request into recent logs array.
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sending Command = " . $command);
		
			//Write commmand to server.
			fwrite($client, $command . "\r\n");
			//Read response from server.
			$response = fread($client,8192);
			
			//Separate message into array.
			$responseArray = explode(",", array_pop(explode("|", $response)));
			if (!strcmp($responseArray[0], "SUCC\0"))
			{
				//Post server response to logs array.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Set Buy Amount Success.");
			}
			else
			{
				//Failed, post error message.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Set Buy Amount Command Failed; " . $responseArray[1]);
			}
		}
		else
		{
			//Failed, post error message.
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Set Buy Amount Command Failed, a transaction is pending.");
		}
	}
/******************************************************
Set Buy Trigger
*******************************************************/
	else if (strcmp($_POST['cmd'], "buytrigger") === 0)
	{
		//Get stockid and Format to alphanumeric.
		$stockid = preg_replace("/[^A-Za-z0-9 ]/", '', $_POST['stockid']);
		//Get amount and format to standard dollar value without symbols.
		$amount = preg_replace("/[^0-9.]/", '', $_POST['amount']);
		$amount = number_format($amount, 2, '.', '');

		//Create user command.
		$command = "WSC|" . $_SESSION["transNum"] . "|SET_BUY_TRIGGER|" . $_SESSION["userid"] . "," . $stockid . "," . $amount;
	
		//Post request into recent logs array.
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sending Command = " . $command);
		
		//Write commmand to server.
		fwrite($client, $command . "\r\n");
		//Read response from server.
		$response = fread($client,8192);
		
		//Separate message into array.
		$responseArray = explode(",", array_pop(explode("|", $response)));
		if (!strcmp($responseArray[0], "SUCC\0"))
		{
			//Post success response to logs array.
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Set Buy Trigger was Successful. ");
		}
		else
		{
			//Failed, post error message.
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Set Buy Trigger FAIL; " . $responseArray[1]);
		}
	}
/******************************************************
Cancel Set Buy
*******************************************************/
	else if (strcmp($_POST['cmd'], "cancelsetbuy") === 0)
	{	
		//Get stockid and Format to alphanumeric.
		$stockid = preg_replace("/[^A-Za-z0-9 ]/", '', $_POST['stockid']);
		//Create user command.
		$command = "WSC|" . $_SESSION["transNum"] . "|CANCEL_SET_BUY|" . $_SESSION["userid"] . "," . $stockid;
		
		//Post request into recent logs array.
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sending Command = " . $command);
		
		//Write commmand to server.
		fwrite($client, $command . "\r\n");
		//Read response from server.
		$response = fread($client,8192);
		
		//Separate message into array.
		$responseArray = explode(",", array_pop(explode("|", $response)));
		if (!strcmp($responseArray[0], "SUCC\0"))
		{
			//Post success response to logs array.
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Cancel Set Buy Successful. ");
		}
		else
		{
			//Failed, post error message.
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Cancel Set Buy FAIL; " . $responseArray[1]);
		}
	}

	//Close the connection.
	fclose($client);
	
	//Redirect back to dashboard.
	header('Location: dashboard.php'); 
?>

