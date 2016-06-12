var logged_in = false;
var is_patching = false;
var patch_paused = false;
var patchprogress = 0;
var patchinterval;

var cookies;
var authenticateCookies;

function GetAccountInfo()
{
	XSGetInfoGameforge();
}

function XSGetInfoGameforge()
{
	authenticateCookies = authenticateResponse.GetCookies();
					
	var getInfo = CreateXSRequest();
					
	getInfo.AddCookies(cookies);
	getInfo.AddCookies(authenticateCookies);
	getInfo.SetMethod("GET");
	getInfo.SetURL("https://account.tera.gameforge.com/launcher/1/account_server_info?attach_auth_ticket=1");
					
	var response = getInfo.GetResponse(
		function(getInfoResponse)
		{
			getInfoResponse.GetRequest().Release();
			getInfoResponse.Release();
		},
		function(getInfoResponse)
		{
			var downloadData = getInfoResponse.GetDownloadData();
			
			if(!SetAccountData(downloadData))
			{
				Logout();
			}
		}
	);
	
	return false;
}

function XSLoginGameforge()
{
	OnLoginStart();
	
	var email = document.getElementById("IDEmail").value;
	var request = CreateXSRequest();
	
	request.SetMethod("GET");
	request.SetURL("https://account.tera.gameforge.com/launcher/1/signin?lang=en&email=" + email + "&kid=");
	
	var response = request.GetResponse(
		function(resp)
		{
			cookies = resp.GetCookies();
			var authenticate = CreateXSRequest();
			
			var email = document.getElementById("IDEmail").value;
			var pw = document.getElementById("IDPassword").value;
			
			authenticate.SetMethod("POST");
			authenticate.SetURL("https://account.tera.gameforge.com/launcher/1/authenticate");
			authenticate.AddCookies(cookies);
			authenticate.SetPostData("uft8=%E2%9C%93&user[client_time]=Thu Mar 20 2016 15:00:00 GMT+0100 (Central Standard Time)&user[io_black_box]=TERA&game_id=1&user[email]=" + email + "&user[password]=" + pw);
			
			var response = authenticate.GetResponse(
				function(authenticateResponse)
				{
					authenticateCookies = authenticateResponse.GetCookies();
					
					var getInfo = CreateXSRequest();
					
					getInfo.AddCookies(cookies);
					getInfo.AddCookies(authenticateCookies);
					getInfo.SetMethod("GET");
					getInfo.SetURL("https://account.tera.gameforge.com/launcher/1/account_server_info?attach_auth_ticket=1");
					
					var response = getInfo.GetResponse(
						function(getInfoResponse)
						{
							getInfoResponse.GetRequest().Release();
							getInfoResponse.Release();
						},
						function(getInfoResponse)
						{
							var downloadData = getInfoResponse.GetDownloadData();
							
							if(!SetAccountData(downloadData))
							{
								LoginResult(false);
							}
							else
							{
								LoginResult(true);
							}
						}
					);
					
					authenticateResponse.GetRequest().Release();
					authenticateResponse.Release();
				}
			);
			
			resp.GetRequest().Release();
			resp.Release();
		}
	);
		
	return false;
}

$(function()
{
	setProgress(50)
});

function OnLoginStart()
{
	var elements = document.getElementsByClassName("container");
		
	for(var i=0; i < elements.length; i++) 
	{ 
		elements[i].style.opacity = "0.1";
	}
		
	var elements = document.getElementsByClassName("inner-container");
	elements[0].style.visibility = "hidden";
		
	document.getElementById("demo").style.visibility = "visible";	
}

function DoLogin()
{
	if(logged_in)
	{
		StartGame();
		return false;
	}
	
	if(OnLogin(document.getElementById("IDEmail").value, document.getElementById("IDPassword").value))
	{
		OnLoginStart();
	}
	
	return false;
}

function Logout()
{
	logged_in = false;
	
	document.getElementById("button_main").innerHTML = "Login";
	document.getElementById("button_main").setAttribute('onclick', 'return XSLoginGameforge()');
		
	var fields = document.getElementsByClassName("input-field half");
		
	for(var i=0; i < fields.length; i++) 
	{ 
		fields[i].style.display="initial";
	}
		
	var containers = document.getElementsByClassName("inner-container");
	containers[0].style.height="250px";
}

function DoStart()
{
	if(!OnStart())
	{
		alert("Failed to start Tera.");
	}
	
	return false;
}

function DoPatch()
{
	document.getElementById("button_patch").style.display="none";
	document.getElementById("patch").style.visibility="visible";
	
	var elements = document.getElementsByClassName('inner-container');
	elements[1].style.height="250px";
	
	if(OnPatch())
	{
		patchinterval = setInterval(function()
		{
			GetPatchStatus()
		}, 1000);
	}
	
	return false;
}

function LoginResult(result)
{
	var elements = document.getElementsByClassName("container");
		
	for(var i=0; i < elements.length; i++) 
	{ 
		elements[i].style.opacity = "1";
	}
		
	var containers = document.getElementsByClassName("inner-container");
	containers[0].style.visibility = "visible";
	
	document.getElementById("demo").style.visibility = "hidden";

	if(result == true)
	{
		logged_in = true;
		
		document.getElementById("button_main").innerHTML = "Start";
		document.getElementById("button_main").setAttribute('onclick', 'return DoStart()');
		
		var fields = document.getElementsByClassName("input-field half");
		
		for(var i=0; i < fields.length; i++) 
		{ 
			fields[i].style.display="none";
		}
		
		containers[0].style.height="200px";
	}
	else
	{
		logged_in = false;
		alert("Login failed.");
	}
}

$(function()
{
	$('.progressbar').each(function()
	{
		var t = $(this);
		var dataperc = t.attr('data-perc'),
		barperc = Math.round(dataperc*3);
		
		t.find('.label').append('<div class="perc"></div>');
	});
});

function SetProgress(value)
{
	$(function() 
	{
		$('.progressbar').each(function(){
			var t = $(this);
			var perc = Math.round(parseInt(value));
			var labelpos = (parseInt(value)*3);
			t.find('.label').css('left', labelpos);
			t.find('.perc').text(perc+'%');
			t.find('.bar').css('width', value*3);
		});
	});
}

function SetProgressMessage(msg)
{
	document.getElementById("message").value += msg + "\n";
}

function FinishPatch()
{
	SetProgress(100);	
	clearInterval(patchinterval);
}

function ClearMessages()
{
	document.getElementById("message").value = "";
}

(function($) {
  var icon = $('.c-pp__icon');
  
   $('.c-pp__icon').on('click', function() {
     $(this).toggleClass('active');
	 patch_paused = patch_paused ? false : true;
	 if(patch_paused)
	 {
	    ClearMessages();
		SetProgressMessage("Patience...");
		PausePatch();
		clearInterval(patchinterval);
	 }
	 else
	 {
		ResumePatch();
		patchinterval = setInterval(function()
		{
			GetPatchStatus()
		}, 1000);
	 }
   });
  
})(jQuery);
