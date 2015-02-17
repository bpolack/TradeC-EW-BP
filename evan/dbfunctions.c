static void
exit_nicely(PGconn *conn, PGresult   *res)
{
    PQclear(res);
    PQfinish(conn);
    exit(1);
}

//######## Request DB account balance ########
    
//Select row where user_id=user and check if does not exist
res = PQexec(conn, selectRequest);
if (PQresultStatus(res) != PGRES_COMMAND_OK)
{
	/*Socket code to respond to
	  client with User DNE error*/
}

selectedBalance = PQgetvalue(res, 0, 2);
PQclear(res);
/*Socket code to respond to
  client with selectedBalance*/


//######## DB account balance adjustment ########

//Update predefined user balance (performed after a balance request)
res = PQexec(conn, updateRequest);
if (PQresultStatus(res) != PGRES_COMMAND_OK)
{
	//Socket code to report update error
}
else
{
	//Update success
}
PQclear(res);

//######## Insert new row into DB Table (Can be any table or # of rows) ########

res = PQexec(conn, insertTable);
if (PQresultStatus(res) != PGRES_COMMAND_OK)
{
	fprintf(stderr, "INSERT failed: %s", PQerrorMessage(conn));
}
PQclear(res);

//Respond to client with success?
