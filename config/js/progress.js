$(function() {
		
	$('.progressbar').each(function(){
		var t = $(this);
		var dataperc = t.attr('data-perc'),
				barperc = Math.round(dataperc*2.975);
		t.find('.bar').animate({width:barperc}, dataperc*25);
		t.find('.label').append('<div class="perc"></div>');
		
		function perc() {
			var length = t.find('.bar').css('width'),
				perc = Math.round(parseInt(length)/2.975),
				labelpos = (parseInt(length)-2);
			t.find('.label').css('left', labelpos);
			t.find('.perc').text(perc+'%');
		}
		perc();
		setInterval(perc, 110); 
	});
});