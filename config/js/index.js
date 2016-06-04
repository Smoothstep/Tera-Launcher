$('input').focus(function(){
  var label = $("[for='" + $(this).attr('id') + "']")
  label.addClass('raised highlight')
})

$('input').blur(function(){
  var label = $("[for='" + $(this).attr('id') + "']")
  label.removeClass('highlight')
  if($(this).val().length == 0){
    label.removeClass('raised')
  }
})
