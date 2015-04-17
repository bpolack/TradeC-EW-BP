<?php
	session_start();
	if(!isset($_SESSION["userid"])) {
	    // session isn't started
		session_unset();
		session_destroy(); 
	    header('Location: index.html');
	}
	//Implode log array into <br> separated string.	
	$_SESSION["recent"] = implode("<br>", array_slice($_SESSION["recentarray"], 0, 25) );
?>

<!DOCTYPE html>
<html class="no-js" lang="en"><head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
    <meta charset="utf-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
    <title>Admin Commander - DayTrading User Page</title>
    <meta name="author" content="Braighton Polack">
    <meta name="description" content="DayTrading User Page">
    <meta name="viewport" content="user-scalable=0,width=device-width,initial-scale=1,maximum-scale=1">
    
    <link rel="stylesheet" href="style.css">
    <link rel="stylesheet" href="droplistmenu.css">
    <link href='http://fonts.googleapis.com/css?family=Noto+Sans:400,700' rel='stylesheet' type='text/css'>
    <link rel="shortcut icon" href="../favicon.ico" />
    <script src="jquery.min.js"></script>
	<script type="text/javascript" src="commander.js"></script>	

	<!-- Add fancyBox -->
	<link rel="stylesheet" href="fancybox/source/jquery.fancybox.css" type="text/css" media="screen" />
	<script type="text/javascript" src="fancybox/source/jquery.fancybox.pack.js"></script>
    
</head>

<body>
<div class="wrapper">

	<header>
        <section>
        	<img src="img/tradeicon.png" alt="Trade Logo">
            <h1>Stock Commander</h1>
            <h3>An intranet control software by Group 2 &copy;2015.</h3>
        </section>
	</header>
    
    <nav>
    <input type="checkbox" name="openclose" id="menuopenclose" class="inputmenu">
    <label class="menuopenclose" for="menuopenclose" title="Open/Close Menu">Menu</label>
    <input type="radio" name="submenu" id="sub0" class="inputmenu">
    <input type="radio" name="topmenu" id="top0" class="inputmenu">
    <input type="radio" name="topmenu" id="top1" class="inputmenu">
    <input type="radio" name="topmenu" id="top2" class="inputmenu">
    <input type="radio" name="topmenu" id="top3" class="inputmenu">
    <input type="radio" name="topmenu" id="top4" class="inputmenu">
    <label for="top0" class="closetop"></label>
    <ul class="menu">
            <li><span><a href="logout.php">Log Out</a></span></li>
            <li><span><a href="dashboard.php">Dashboard</a></span></li>
            <li><span><a href="accountsummary.php">Account Summary</a></span></li>
            <li><span><a href="dumplog.php">Dumplog</a></span></li>
    </ul>
    <div class="clear"></div>
    </nav>
	
    
    
    
	<section class="content">
        <div class="inner">
            <article class="main">
                <h1>DayTrading - Dump Logs Page</h1>
		<h2>Active User: <?php echo $_SESSION["userid"]; ?></h2>
		<div class="server">
			<?php if($_SESSION["selectedServer"] != null){echo "<div class='calm'>".$_SESSION["serverErr"]."</div>";}
				else{echo "<div class='angry'>".$_SESSION["serverErr"]."</div>";} ?>
		</div>
		<FORM name="dumpall" method="POST" action="dump.php">
			<br>
			<TABLE BORDER="0">
			<input type="hidden" name="cmd" value="dumpall"/>
				<TR>
					<TD><label for="stockid">Filename</label></TD>
					<TD><input type="text" name="filename" maxlength="15" pattern=".{1,}" required style="width:120px;"/></TD>
				</TR>
			</TABLE>
			<P><input type="SUBMIT" value="Dump all Logs"></P>
			<br>
		</FORM>
		<FORM name="dumpme" method="POST" action="dump.php">
			<TABLE BORDER="0">
			<input type="hidden" name="cmd" value="dumpme"/>
				<TR>
					<TD><label for="stockid">Filename</label></TD>
					<TD><input type="text" name="filename" maxlength="15" pattern=".{1,}" required style="width:120px;"/></TD>
				</TR>
			</TABLE>
			<P><input type="SUBMIT" value="Dump my Logs"></P>
		</FORM>
		
		<h2>Recent Actions</h2>
		<div class="recent">
		<?php echo $_SESSION["recent"]; ?>
		</div>

		<h2>Information</h2>
                <p>Logs will be dumped to the hard drive of the log server, and named logfile#.xml.</p>

            </article>
        </div>
        <br clear="all">
	</section>
	
    
    <footer>
        <section>
            <p>Powered by Group 2 - Code, Design, Explore.</p>
        </section>
    </footer>	
	
	
</div>
</body>
</html>
