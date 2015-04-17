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
Sell
*******************************************************/
	if (strcmp($_POST['cmd'], "sell") === 0)
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
			$command = "WSC|" . $_SESSION["transNum"] . "|SELL|" . $_SESSION["userid"] . "," . $stockid . "," . $amount;
		
			//Post sell request into recent logs array.
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
				//Allow Confirm/Cancel sell.
				$_SESSION["sellPending"] = 1;
				
				//Post server response to logs array.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Selling " . $responseArray[1] . " stocks of " . $stockid . " @ $" . $responseArray[2] . "each. Seconds to confirm = " . ($_SESSION["timeEnd"] - time()));
				//Post pending message to session var.
				$_SESSION["pending"] = "To sell " . $responseArray[1] . " stocks of " . $stockid . " @ $" . $responseArray[2] . "each.";
			}
			else
			{
				//Sell Failed, post error message.
				$_SESSION["sellPending"] = 0;
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sell Command Failed; " . $responseArray[1]);
			}
		}
		else
		{
			//Sell Failed, post error message.
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sell Command Failed, another transaction is pending.");
		}
	}
/******************************************************
Commit Sell
*******************************************************/
	else if (strcmp($_POST['cmd'], "commitsell") === 0)
	{
		if (($_SESSION["sellPending"] == 1) && ($_SESSION["timeEnd"] > time()))
		{
			//Create user command.
			$command = "WSC|" . $_SESSION["transNum"] . "|COMMIT_SELL|" . $_SESSION["userid"];
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sending Commit Sell Command. ");
			//Write commmand to server.
			fwrite($client, $command . "\r\n");
			//Read response from server.
			$response = fread($client,8192);
			
			//Separate message into array.
			$responseArray = explode(",", array_pop(explode("|", $response)));
			if (!strcmp($responseArray[0], "SUCC\0"))
			{
				//Commit Sell Successful.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sell Committed Successfully. ");
			}
			else
			{
				//Commit Sell FAIL.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Sell Commit FAIL; " . $responseArray[1]);
			}
			$_SESSION["sellPending"] = 0;
			$_SESSION["timeEnd"] = null;
			$_SESSION["pending"] = "No Pending Command";
		}
		else if (($_SESSION["sellPending"] == 1) && ($_SESSION["timeEnd"] <= time()))
		{
			//Create user command.
			$command = "WSC|" . $_SESSION["transNum"] . "|CANCEL_SELL|" . $_SESSION["userid"];
			//Write commmand to server.
			fwrite($client, $command . "\r\n");
			//Read response from server.
			$response = fread($client,8192);
			
			//Separate message into array.
			$responseArray = explode(",", array_pop(explode("|", $response)));
			if (!strcmp($responseArray[0], "SUCC\0"))
			{
				//Cancel Sell Successful.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Pending Sell Action Canceled. Quote Timeout. ");
			}
			else
			{
				//Cancel Sell FAIL.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Pending Sell Action Canceled. Quote Timeout. ACTION FAIL; " . $responseArray[1]);
			}
			$_SESSION["sellPending"] = 0;
			$_SESSION["timeEnd"] = null;
			$_SESSION["pending"] = "No Pending Command";
		}
		else
		{
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : No Pending Sell Action.");
		}
	}
/******************************************************
Cancel Sell
*******************************************************/
	else if (strcmp($_POST['cmd'], "cancelsell") === 0)
	{
		if ($_SESSION["sellPending"] == 1)
		{
			//Create user command.
			$command = "WSC|" . $_SESSION["transNum"] . "|CANCEL_SELL|" . $_SESSION["userid"];
			//Write commmand to server.
			fwrite($client, $command . "\r\n");
			//Read response from server.
			$response = fread($client,8192);
			
			//Separate message into array.
			$responseArray = explode(",", array_pop(explode("|", $response)));
			if (!strcmp($responseArray[0], "SUCC\0"))
			{
				//Cancel Sell Successful.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Pending Sell Action Canceled. ");
			}
			else
			{
				//Cancel Sell FAIL.
				array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : Cancel action FAIL; " . $responseArray[1]);
			}
			//Cancel Sell Pending.
			$_SESSION["sellPending"] = 0;
			$_SESSION["timeEnd"] = null;
			$_SESSION["pending"] = "No Pending Command";
		}
		else
		{
			array_unshift($_SESSION["recentarray"], date("h:i:sa") . " : No Pending Sell Action.");
		}
	}

	//Close the connection.
	fclose($client);
	
	//Redirect back to dashboard.
	header('Location: dashboard.php'); 
?>

