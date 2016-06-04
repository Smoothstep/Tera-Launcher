var logged_in = false;
var is_patching = false;
var patch_paused = false;
var patchprogress = 0;
var patchinterval;

$(function()
{
	setProgress(50)
});

function DoLogin()
{
	if(logged_in)
	{
		StartGame();
		return false;
	}
	
	if(OnLogin(document.getElementById("IDEmail").value, document.getElementById("IDPassword").value))
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
	
	return false;
}

function Logout()
{
	logged_in = false;
	
	document.getElementById("button_main").innerHTML = "Login";
	document.getElementById("button_main").setAttribute('onclick', 'return DoLogin()');
		
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
		alert("Could not start Tera.");
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

function setProgress(value)
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

function setProgressMessage(msg)
{
	document.getElementById("message").value += msg + "\n";
}

function finishPatch()
{
	setProgress(100);	
	clearInterval(patchinterval);
}

function ClearMessages()
{
	document.getElementById("message").value = "";
}

function MakeAlert()
{
}

(function($) {
  var icon = $('.c-pp__icon');
  
   $('.c-pp__icon').on('click', function() {
     $(this).toggleClass('active');
	 patch_paused = patch_paused ? false : true;
	 if(patch_paused)
	 {
	    ClearMessages();
		setProgressMessage("Patience...");
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
