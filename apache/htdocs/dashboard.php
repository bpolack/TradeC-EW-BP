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
            <li><span><label for="top2">Commands</label><label for="top0"></label></span>
                
                <div>
                	<input type="radio" name="submenu" id="sub1" class="inputmenu">
                    <dl class="dl1">
                        <dt><label for="sub1">Account</label><label for="sub0"></label></dt>
                        <dd>
                			<a class="fancybox" href="#addForm">Add Funds</a>
                            <a class="fancybox" href="accountsummary.php">Account Summary</a>
                        </dd>
                    </dl>
                    <input type="radio" name="submenu" id="sub2" class="inputmenu">
                    <dl class="dl2">
                        <dt><label for="sub2">Quote Server</label><label for="sub0"></label></dt>
                        <dd>
                    		<a class="fancybox" href="#quoteForm">Get Quote</a>
                        </dd>
                    </dl>
                    <input type="radio" name="submenu" id="sub3" class="inputmenu">
                    <dl class="dl3">
                        <dt><label for="sub3">Buy Options</label><label for="sub0"></label></dt>
                        <dd>
                        	<a class="fancybox" href="#buyForm">Buy Stocks</a>
                            <a class="fancybox" href="#commitbuyForm">Commit Buy</a>
                            <a class="fancybox" href="#cancelbuyForm">Cancel Buy</a>
                            <a class="fancybox" href="#setbuyForm">Set Buy Amount</a>
                            <a class="fancybox" href="#buytriggerForm">Set Buy Trigger</a>
                            <a class="fancybox" href="#cancelsetbuyForm">Cancel Set Buy</a>
                        </dd>
                    </dl>
                    <input type="radio" name="submenu" id="sub4" class="inputmenu">
                    <dl class="dl4">
                        <dt><label for="sub4">Sell Options</label><label for="sub0"></label></dt>
                        <dd>
                        	<a class="fancybox" href="#sellForm">Sell Stocks</a>
                            <a class="fancybox" href="#commitsellForm">Commit Sell</a>
                            <a class="fancybox" href="#cancelsellForm">Cancel Sell</a>
                            <a class="fancybox" href="#setsellForm">Set Sell Amount</a>
                            <a class="fancybox" href="#selltriggerForm">Set Sell Trigger</a>
                            <a class="fancybox" href="#cancelsetsellForm">Cancel Set Sell</a>
                        </dd>
                    </dl>
                </div>
                
            </li>
            <li><span><a href="accountsummary.php">Account Summary</a></span></li>
            <li><span><a href="dumplog.php">Dumplog</a></span></li>
        </ul>
        <div class="clear"></div>
    </nav>
	
    
    
    
	<section class="content">
        <div class="inner">
            <article class="main">
                <h1>DayTrading - User Control Page</h1>
		<h2>Active User: <?php echo $_SESSION["userid"]; ?></h2>
		<div class="server">
			<?php if($_SESSION["selectedServer"] != null){echo "<div class='calm'>".$_SESSION["serverErr"]."</div>";}
				else{echo "<div class='angry'>".$_SESSION["serverErr"]."</div>";} ?>
		</div>
		<h2>Recent Commands</h2>
		<div class="recent">
			<?php echo $_SESSION["recent"]; ?>
		</div>
		<h2>Pending Action</h2>
		<div class="pending">
                    <form name="counter"><input type="text" size="8" 
                    name="d2"> Seconds Remaining</form> 
                    <script> 
                        <!-- 
                        // Function to perform countdown to specified time.
                        var target=<?php echo $_SESSION["timeEnd"]; ?> 
                        var start=Math.floor((new Date).getTime()/1000) 
                        var milisec=0 
                        var seconds=target-start 
                        document.counter.d2.value=seconds+"."+milisec
            
                        function display(){ 
                        if (milisec<=0){ 
                        milisec=9 
                        seconds-=1 
                        } 
                        if (seconds<=-1){ 
                        milisec=0 
                        seconds+=1 
                        } 
                        else 
                        milisec-=1 
                        document.counter.d2.value=seconds+"."+milisec 
                        setTimeout("display()",100) 
                        } 
                        display() 
                        --> 
                    </script> 
			<?php echo $_SESSION["pending"]; ?>
		</div>
				
                
                <h2>Information</h2>
                <p>This is a multifunction control page suitable for all modern browsers and OS, including IE10+, touch screens, tablets and smartphones. When viewed on a small screen size, the menu changes to a single column slide down.</p>
                <p>Use the menu above to access several stock command functions. <b>Buy and sell stocks</b> for the current user, <b>add funds, get quotes, and view account information.</b></p>
                <p>If a sell or buy is in progress, a <b>timer will be displayed</b> in the top left corner to display the remaining time to commit the command.</p>
                <p>This page may not display correctly in Internet Explorer versions lower then IE9.</p>
            </article>
        </div>
        <br clear="all">
	</section>

	<!-- FancyBox Command Prompts - Not Displayed by Default -->
	<div style="display:none">
	<div class="commandForm" id="addForm">
		<h2>Add Funds</h2>
		
		<FORM name="add" method="POST" action="add.php">
			<TABLE BORDER="0">
				<TR>
					<TD><label for="amount">Amount</label></TD>
					<TD><input type="text" name="amount" maxlength="10" required style="width:120px;"/></TD>
				</TR>
			</TABLE>
			<BR>
			<P><input type="SUBMIT" value="Submit"></P>
		</FORM>
	</div>
	</div>
	<div style="display:none">
	<div class="commandForm" id="quoteForm">
		<h2>Get a Quote</h2>
		
		<FORM name="quote" method="POST" action="quote.php">
			<TABLE BORDER="0">
				<TR>
					<TD><label for="stockid">Stock Symbol</label></TD>
					<TD><input type="text" name="stockid" maxlength="3" pattern=".{1,}" required style="width:120px;"/></TD>
				</TR>
			</TABLE>
			<BR>
			<P><input type="SUBMIT" value="Submit"></P>
		</FORM>
	</div>
	</div>
	<div style="display:none">
	<div class="commandForm" id="buyForm">
		<h2>Place a Buy Order</h2>
		
		<FORM name="buy" method="POST" action="buy.php">
			<TABLE BORDER="0">
				<input type="hidden" name="cmd" value="buy"/>
				<TR>
					<TD><label for="stockid">Stock Symbol</label></TD>
					<TD><input type="text" name="stockid" maxlength="3" pattern=".{1,}" required style="width:120px;"/></TD>
				</TR>
				<TR>
					<TD><label for="amount">Amount to Spend</label></TD>
					<TD><input type="number" name="amount" min="0.01" step="0.01" max="1000000.00" required style="width:120px;"/></TD>
				</TR>
			</TABLE>
			<BR>
			<P><input type="SUBMIT" value="Submit"></P>
		</FORM>
	</div>
	</div>

	<div style="display:none">
	<div class="commandForm" id="commitbuyForm">
		<h2>Commit the Pending Buy Order</h2>
		
		<FORM name="commitbuy" method="POST" action="buy.php">
			<input type="hidden" name="cmd" value="commitbuy"/>
			<BR>
			<P><input type="SUBMIT" value="Commit Order"></P>
		</FORM>
	</div>
	</div>

	<div style="display:none">
	<div class="commandForm" id="cancelbuyForm">
		<h2>Cancel the Pending Buy Order</h2>
		
		<FORM name="cancelbuy" method="POST" action="buy.php">
			<input type="hidden" name="cmd" value="cancelbuy"/>
			<BR>
			<P><input type="SUBMIT" value="Cancel Order"></P>
		</FORM>
	</div>
	</div>

	<div style="display:none">
	<div class="commandForm" id="setbuyForm">
		<h2>Set Buy Amount for an Automatic Buy</h2>
		
		<FORM name="setbuy" method="POST" action="setbuy.php">
			<TABLE BORDER="0">
				<input type="hidden" name="cmd" value="setbuy"/>
				<TR>
					<TD><label for="stockid">Stock Symbol</label></TD>
					<TD><input type="text" name="stockid" maxlength="3" pattern=".{1,}" required style="width:120px;"/></TD>
				</TR>
				<TR>
					<TD><label for="amount">Amount to Spend</label></TD>
					<TD><input type="number" name="amount" min="0.01" step="0.01" max="1000000.00" required style="width:120px;"/></TD>
				</TR>
			</TABLE>
			<BR>
			<P><input type="SUBMIT" value="Submit"></P>
		</FORM>
	</div>
	</div>

	<div style="display:none">
	<div class="commandForm" id="buytriggerForm">
		<h2>Set Trigger for an Automatic Buy</h2>
		
		<FORM name="buytrigger" method="POST" action="setbuy.php">
			<TABLE BORDER="0">
				<input type="hidden" name="cmd" value="buytrigger"/>
				<TR>
					<TD><label for="stockid">Stock Symbol</label></TD>
					<TD><input type="text" name="stockid" maxlength="3" pattern=".{1,}" required style="width:120px;"/></TD>
				</TR>
				<TR>
					<TD><label for="amount">Trigger Stock Value</label></TD>
					<TD><input type="number" name="amount" min="0.01" step="0.01" max="1000000.00" required style="width:120px;"/></TD>
				</TR>
			</TABLE>
			<BR>
			<P><input type="SUBMIT" value="Submit"></P>
		</FORM>
	</div>
	</div>

	<div style="display:none">
	<div class="commandForm" id="cancelsetbuyForm">
		<h2>Cancel an Automatic Buy</h2>
		
		<FORM name="cancelsetbuy" method="POST" action="setbuy.php">
			<TABLE BORDER="0">
				<input type="hidden" name="cmd" value="cancelsetbuy"/>
				<TR>
					<TD><label for="stockid">Stock Symbol</label></TD>
					<TD><input type="text" name="stockid" maxlength="3" pattern=".{1,}" required style="width:120px;"/></TD>
				</TR>
			</TABLE>
			<BR>
			<P><input type="SUBMIT" value="Cancel Order"></P>
		</FORM>
	</div>
	</div>

	<div style="display:none">
	<div class="commandForm" id="sellForm">
		<h2>Place a Sell Order</h2>
		
		<FORM name="sell" method="POST" action="sell.php">
			<TABLE BORDER="0">
				<input type="hidden" name="cmd" value="sell"/>
				<TR>
					<TD><label for="stockid">Stock Symbol</label></TD>
					<TD><input type="text" name="stockid" maxlength="3" pattern=".{1,}" required style="width:120px;"/></TD>
				</TR>
				<TR>
					<TD><label for="amount">Amount to Sell</label></TD>
					<TD><input type="number" name="amount" min="0.01" step="0.01" max="1000000.00" required style="width:120px;"/></TD>
				</TR>
			</TABLE>
			<BR>
			<P><input type="SUBMIT" value="Submit"></P>
		</FORM>
	</div>
	</div>

	<div style="display:none">
	<div class="commandForm" id="commitsellForm">
		<h2>Commit the Pending Sell Order</h2>
		
		<FORM name="commitsell" method="POST" action="sell.php">
			<input type="hidden" name="cmd" value="commitsell"/>
			<BR>
			<P><input type="SUBMIT" value="Commit Order"></P>
		</FORM>
	</div>
	</div>

	<div style="display:none">
	<div class="commandForm" id="cancelsellForm">
		<h2>Cancel the Pending Sell Order</h2>
		
		<FORM name="cancelsell" method="POST" action="sell.php">
			<input type="hidden" name="cmd" value="cancelsell"/>
			<BR>
			<P><input type="SUBMIT" value="Cancel Order"></P>
		</FORM>
	</div>
	</div>

	<div style="display:none">
	<div class="commandForm" id="setsellForm">
		<h2>Set Sell Amount for an Automatic Sell</h2>
		
		<FORM name="setsell" method="POST" action="setsell.php">
			<TABLE BORDER="0">
				<input type="hidden" name="cmd" value="setsell"/>
				<TR>
					<TD><label for="stockid">Stock Symbol</label></TD>
					<TD><input type="text" name="stockid" maxlength="3" pattern=".{1,}" required style="width:120px;"/></TD>
				</TR>
				<TR>
					<TD><label for="amount">Amount to Sell</label></TD>
					<TD><input type="number" name="amount" min="0.01" step="0.01" max="1000000.00" required style="width:120px;"/></TD>
				</TR>
			</TABLE>
			<BR>
			<P><input type="SUBMIT" value="Submit"></P>
		</FORM>
	</div>
	</div>

	<div style="display:none">
	<div class="commandForm" id="selltriggerForm">
		<h2>Set Trigger for an Automatic Sell</h2>
		
		<FORM name="selltrigger" method="POST" action="setsell.php">
			<TABLE BORDER="0">
				<input type="hidden" name="cmd" value="selltrigger"/>
				<TR>
					<TD><label for="stockid">Stock Symbol</label></TD>
					<TD><input type="text" name="stockid" maxlength="3" pattern=".{1,}" required style="width:120px;"/></TD>
				</TR>
				<TR>
					<TD><label for="amount">Trigger Stock Value</label></TD>
					<TD><input type="number" name="amount" min="0.01" step="0.01" max="1000000.00" required style="width:120px;"/></TD>
				</TR>
			</TABLE>
			<BR>
			<P><input type="SUBMIT" value="Submit"></P>
		</FORM>
	</div>
	</div>

	<div style="display:none">
	<div class="commandForm" id="cancelsetsellForm">
		<h2>Cancel an Automatic Sell</h2>
		
		<FORM name="cancelsetsell" method="POST" action="setsell.php">
			<TABLE BORDER="0">
				<input type="hidden" name="cmd" value="cancelsetsell"/>
				<TR>
					<TD><label for="stockid">Stock Symbol</label></TD>
					<TD><input type="text" name="stockid" maxlength="3" pattern=".{1,}" required style="width:120px;"/></TD>
				</TR>
			</TABLE>
			<BR>
			<P><input type="SUBMIT" value="Cancel Order"></P>
		</FORM>
	</div>
	</div>
	
    
    <footer>
        <section>
            <p>Powered by Group 2 - Code, Design, Explore.</p>
        </section>
    </footer>	
	
	
</div>
</body>
</html>
