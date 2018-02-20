var url;
$(function() {
	url = window.location.href;
	//$("#close-page").on("click",closePage);	
	$("#continue").on("click",continuePage);
})

function setUrl() {
	var data = "whiteFlag=0&url="+url.slice(0,url.length-13);
	$.post("goform/InsertWhite",data);
	
}

function callback(str) {
	if(!top.isTimeout(str)) {
		return;	
	}
	var num = $.parseJSON(str).errCode;
	if(num == 0) {
		window.history.back();
	} else {
		window.location.href = url.slice(0,url.length-13);
	}
}

function continuePage() {
	var data = "whiteFlag=1&url="+url.slice(0,url.length-13);
	$.post("goform/InsertWhite",data,callback);
}

window.onload = function() {
	setUrl();	
}