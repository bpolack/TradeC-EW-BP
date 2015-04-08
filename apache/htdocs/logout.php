<?php
	// Start user session.
	session_start(); 
?>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
    <head>
        <title>Logout Session</title>
    </head>

	<?php
		//Cancel any pending transactions.
		
		//End current session.
		session_unset();
		session_destroy(); 
		echo "Session variables are destroyed.";
		echo "<br> Redirecting to Login Page...";
		header('Location: index.html'); 
    ?>
   

</html>
