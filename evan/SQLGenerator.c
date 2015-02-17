/*******************************************************************/
//Generate SQL Commands for each type of Stock Command
/*******************************************************************/

//String Constants
const char* selectBal = "SELECT balance FROM accounts WHERE user_id = %s";
const char* selectTrans = "SELECT TOP 10 * FROM transactions WHERE user_id = %s";
const char* updateBal = "UPDATE accounts SET balance = balance + %s WHERE user_id = %s"
const char* insertAcct = "INSERT INTO accounts (User_ID,Balance) VALUES (%s, %s)";
const char* insertTran = "INSERT INTO transactions (User_ID,Stock_ID,Adjustment,Timestamp) VALUES (%s, %s, %s, %s)";
const char* insertStk = "INSERT INTO stocks (User_ID,Stock_ID,Amount) VALUES (%s, %s, %d)";
const char* selectStk = "SELECT num_owned FROM stocks WHERE user_id = %s AND stock_id = %s";
const char* selectallStk = "SELECT * FROM stocks WHERE user_id = %s";
const char* updateStk = "UPDATE stocks SET amount = amount + %d WHERE user_id = %s AND stock_id = %s";



//ADD command 

	//request users current balance
	char * s = malloc(snprintf(NULL, 0, selectBal, userid) + 1);
	sprintf(s, selectBal, userid);
	//send request to DBserver
	free(s);
	
	char* bal = malloc(10);
	//get balance from DBserver
	
	if (bal == "DNE")
	{
		//create new user with the amount specified in the command
		char * s = malloc(snprintf(NULL, 0, insertAcct, userid, adjustment) + 1);
		sprintf(s, insertAcct, userid, adjustment);
		//send insert to DBserver
		free(s);
	}
	else
	{
		//update users current balance by the amount specified in the command
		char * s = malloc(snprintf(NULL, 0, updateBal, adjustment, userid) + 1);
		sprintf(s, updateBal, adjustment, userid);
		//send update to DBserver
		free(s);
	}
	
	//create new transaction log
	char * s = malloc(snprintf(NULL, 0, insertTrans, userid, stockid, adjustment, timestamp) + 1);
	sprintf(s, insertTrans, userid, stockid, adjustment, timestamp);
	//send insert to DBserver
	free(s);
	
//BUY command

	//request users current balance
	char * s = malloc(snprintf(NULL, 0, selectBal, userid) + 1);
	sprintf(s, selectBal, userid);
	//send request to DBserver
	free(s);
	
	//get balance from DBserver
	char* bal = 0000000;
	//typecast
	
	if( bal-cost > 0 )
	{
		//Buy is possible, allow COMMIT_BUY
	}
	else
	{
		//Return error log, not enough money
	}

//COMMIT_BUY command

	//update users current balance by the amount specified in the command
	char * s = malloc(snprintf(NULL, 0, updateBal, bal, adjustment, userid) + 1);
	sprintf(s, updateBal, bal, adjustment, userid);
	//send update to DBserver
	free(s);
	
	//create new transaction log
	char * s = malloc(snprintf(NULL, 0, insertTrans, userid, stockid, adjustment, timestamp) + 1);
	sprintf(s, insertTrans, userid, stockid, adjustment, timestamp);
	//send insert to DBserver
	free(s);	
	
	//request users current num_owned of stock XXX
	char * s = malloc(snprintf(NULL, 0, selectStk, userid, stockid) + 1);
	sprintf(s, selectStk, userid, stockid);
	//send request to DBserver
	free(s);
	
	if(num_owned == "DNE")
	{
		//create new row for stock XXX owned by user
		char * s = malloc(snprintf(NULL, 0, insertStk, userid, stockid, numBought) + 1);
		sprintf(s, insertStk, userid, stockid, numBought);
		//send insert to DBserver
		free(s);
	}
	else
	{
		//update users current num_owned by the amount bought/sold for stock XXX
		char * s = malloc(snprintf(NULL, 0, updateStk, num_owned, numBought, userid, stockid) + 1);
		sprintf(s, updateStk, num_owned, numBought, userid, stockid);
		//send update to DBserver
		free(s);
	}

//CANCEL_BUY command


//SELL command

	//request users current num_owned of stock XXX
	char * s = malloc(snprintf(NULL, 0, selectStk, userid, stockid) + 1);
	sprintf(s, selectStk, userid, stockid);
	//send request to DBserver
	free(s);
	
	if( num_owned > 0 )
	{
		//Sell is possible, allow COMMIT_SELL
	}
	else
	{
		//Return error log, not enough stocks to sell
	}

//COMMIT_SELL command
	
	//update users current balance by the amount specified in the command
	char * s = malloc(snprintf(NULL, 0, updateBal, bal, adjustment, userid) + 1);
	sprintf(s, updateBal, bal, adjustment, userid);
	//send update to DBserver
	free(s);
	
	//create new transaction log
	char * s = malloc(snprintf(NULL, 0, insertTrans, userid, stockid, adjustment, timestamp) + 1);
	sprintf(s, insertTrans, userid, stockid, adjustment, timestamp);
	//send insert to DBserver
	free(s);
	
	//update users current num_owned by the amount bought/sold for stock XXX
	char * s = malloc(snprintf(NULL, 0, updateStk, num_owned, numSold, userid, stockid) + 1);
	sprintf(s, updateStk, num_owned, numSold, userid, stockid);
	//send update to DBserver
	free(s);

//CANCEL_SELL command


//SET_BUY_AMOUNT command


//CANCEL_SET_BUY command


//SET_BUY_TRIGGER command


//SET_SELL_AMOUNT command


//SET_SELL_TRIGGER command


//CANCEL_SET_SELL command


//DUMPLOG command


//DISPLAY_SUMMARY command

	//request users current balance
	char * s = malloc(snprintf(NULL, 0, selectBal, userid) + 1);
	sprintf(s, selectBal, userid);
	//send request to DBserver
	free(s);
	
	//request list of user's owned stocks
	char * s = malloc(snprintf(NULL, 0, selectallStk, userid) + 1);
	sprintf(s, selectallStk, userid);
	//send request to DBserver
	free(s);	

	//request list of user's recent transactions
	char * s = malloc(snprintf(NULL, 0, selectTrans, userid) + 1);
	sprintf(s, selectTrans, userid);
	//send request to DBserver
	free(s);


