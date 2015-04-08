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
Buy
*******************************************************/
	if (strcmp($_POST['cmd'], "buy") === 0)
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
			$command = "WSC|" . $_SESSION["transNum"] . "|BUY|" . $_SESSION["userid"] . "," . $stockid . "," . $amount;
		
			//Post buy request into recent logs array.
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sending Command = " . $command);
		
			//Write commmand to server.
			fwrite($client, $command . "\r\n");
			//Read response from server.
			$response = fread($client,8192);
			
			//Separate message into array.
			$responseArray = explode(",", array_pop(explode("|", $response)));
			if (!strcmp($responseArray[0], "SUCC"))
			{
				//Create Timing Variables.
				$_SESSION["timeEnd"] = 30 + floor(($responseArray[3])/1000);
				//Allow Confirm/Cancel Buy.
				$_SESSION["buyPending"] = 1;
				
				//Post server response to logs array.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Buying " . $responseArray[1] . " stocks of " . $stockid . " @ $" . $responseArray[2] . "each. Seconds to confirm = " . ($_SESSION["timeEnd"] - time()));
				//Post pending message to session variable.
				$_SESSION["pending"] = "To buy " . $responseArray[1] . " stocks of " . $stockid . " @ $" . $responseArray[2] . "each.";
			}
			else
			{
				//Buy Failed, post error message.
				$_SESSION["buyPending"] = 0;
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Buy Command Failed; " . $responseArray[1]);
			}
		}
		else
		{
			//Buy Failed, post error message.
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Buy Command Failed, another transaction is pending.");
		}
	}
/******************************************************
Commit Buy
*******************************************************/
	else if (strcmp($_POST['cmd'], "commitbuy") === 0)
	{
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Time - " . time());
		array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : End Time - " . $_SESSION["timeEnd"]);
		if (($_SESSION["buyPending"] == 1) && ($_SESSION["timeEnd"] > time()))
		{
			//Create user command.
			$command = "WSC|" . $_SESSION["transNum"] . "|COMMIT_BUY|" . $_SESSION["userid"];
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sending Commit Buy Command. ");
			//Write commmand to server.
			fwrite($client, $command . "\r\n");
			//Read response from server.
			$response = fread($client,8192);
			
			//Separate message into array.
			$responseArray = explode(",", array_pop(explode("|", $response)));
			if (!strcmp($responseArray[0], "SUCC\0"))
			{
				//Post success response to logs array.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Buy Committed Successfully. ");
			}
			else
			{
				//Commit Buy Failed, post error message.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Buy Commit FAIL; " . $responseArray[1]);
			}
			$_SESSION["buyPending"] = 0;
			$_SESSION["timeEnd"] = null;
			$_SESSION["pending"] = "No Pending Command";
		}
		else if (($_SESSION["buyPending"] == 1) && ($_SESSION["timeEnd"] <= time()))
		{
			//Create user command.
			$command = "WSC|" . $_SESSION["transNum"] . "|CANCEL_BUY|" . $_SESSION["userid"];
			//Write commmand to server.
			fwrite($client, $command . "\r\n");
			//Read response from server.
			$response = fread($client,8192);
			
			//Separate message into array.
			$responseArray = explode(",", array_pop(explode("|", $response)));
			if (!strcmp($responseArray[0], "SUCC\0"))
			{
				//Post success response to logs array.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Pending Buy Action Canceled. Quote Timeout. ");
			}
			else
			{
				//Cancel Buy Failed, post error message.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Pending Buy Action Canceled. Quote Timeout. ACTION FAIL; " . $responseArray[1]);
			}
			$_SESSION["buyPending"] = 0;
			$_SESSION["timeEnd"] = null;
			$_SESSION["pending"] = "No Pending Command";
		}
		else
		{
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : No Pending Buy Action.");
		}
	}
/******************************************************
Cancel Buy
*******************************************************/
	else if (strcmp($_POST['cmd'], "cancelbuy") === 0)
	{
		if ($_SESSION["buyPending"] == 1)
		{
			//Create user command.
			$command = "WSC|" . $_SESSION["transNum"] . "|CANCEL_BUY|" . $_SESSION["userid"];
			//Write commmand to server.
			fwrite($client, $command . "\r\n");
			//Read response from server.
			$response = fread($client,8192);
			
			//Separate message into array.
			$responseArray = explode(",", array_pop(explode("|", $response)));
			if (!strcmp($responseArray[0], "SUCC\0"))
			{
				//Post success response to logs array.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Pending Buy Action Canceled. ");
			}
			else
			{
				//Cancel Buy Failed, post error message.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Cancel action FAIL; " . $responseArray[1]);
			}
			//Cancel Buy Pending
			$_SESSION["buyPending"] = 0;
			$_SESSION["timeEnd"] = null;
			$_SESSION["pending"] = "No Pending Command";
		}
		else
		{
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : No Pending Buy Action.");
		}
	}

	//Close the connection
	fclose($client);
	
	//Redirect back to dashboard.
	header('Location: dashboard.php'); 
?>


