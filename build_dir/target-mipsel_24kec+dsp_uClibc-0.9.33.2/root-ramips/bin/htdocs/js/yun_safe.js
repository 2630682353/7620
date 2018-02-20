// JavaScript Document

var url, mac;
$(function() {
	var array1 = window.location.href.split("?");
	var arg_str = array1[1];
	var arg_array;

	for (i = 2; i < array1.length; i++) {
		arg_str += "?";
		arg_str += array1[i];
	}

        //alert(arg_str);
	arg_array = arg_str.split("&");

	//http://192.168.0.1/yun_safe.html?mac=>>>&domain=
	mac = arg_array[0].split("=")[1];
	var url_array = arg_array[1].split("=");
	url = url_array[1];
	for (i = 2; i < url_array.length; i++) {
		url += "=";
		url += url_array[i];
	}

	for (i = 2; i < arg_array.length; i++) {
		url += "&";
		url += arg_array[i];
	}
	//alert(mac);
	//alert(url);

	$("#continue_open").on("click",continuePage);

	$("#return_back").on("click",returnBackPage);

	
})

function returnBackPage(){
	history.go(-1);
}

function returnBack(data) {
	//alert("http://" + url);
	window.location = "http://" + url;
}



function continuePage() {
	var data = "mac=" + mac + "&domain=" + url;
	$.post("goform/InsertWhite",data, returnBack);
}

