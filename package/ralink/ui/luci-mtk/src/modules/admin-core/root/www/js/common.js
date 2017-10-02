	var commonInterval = 2000;
	var MODEL = new Array("其它","华为","小米","魅族","磊科","联想","长虹","HTC","苹果","三星","诺基亚","索尼","酷派","OPPO","LG","中兴","步步高VIVO","摩托罗拉","金立","天语","TCL","飞利浦","戴尔","惠普","华硕","东芝","宏碁","海信","腾达","TPLINK","一加","锤子","瑞昱","清华同方","触云","创维","康佳","乐视","海尔","夏普","VMware","Intel","技嘉","海华","其它","其它","其它","其它","仁宝","其它","其它","其它","其它","谷歌","天珑","泛泰","其它","其它","微软");
	var actionUrl = document.domain;
	if(actionUrl != null && actionUrl != ""){
		actionUrl = "http://"+actionUrl+"/protocol.csp?"
	}else{
		alert("获取路由器网关IP失败！");
	}
 
	var browser = { 
		versions:function(){ 
			var u = navigator.userAgent, app = navigator.appVersion; 
			return {
				trident: u.indexOf('Trident') > -1, //IE内核 
				presto: u.indexOf('Presto') > -1, //opera内核 
				webKit: u.indexOf('AppleWebKit') > -1, //苹果、谷歌内核 
				gecko: u.indexOf('Gecko') > -1 && u.indexOf('KHTML') == -1, //火狐内核 
				mobile: !!u.match(/AppleWebKit.*Mobile.*/), //是否为移动终端 
				ios: !!u.match(/\(i[^;]+;( U;)? CPU.+Mac OS X/), //ios终端 
				android: u.indexOf('Android') > -1 || u.indexOf('Linux') > -1, //android终端或uc浏览器 
				iPhone: u.indexOf('iPhone') > -1 , //是否为iPhone或者QQHD浏览器 
				iPad: u.indexOf('iPad') > -1, //是否iPad 
				webApp: u.indexOf('Safari') == -1 //是否web应该程序，没有头部与底部 
			}; 
		}(), 
		language:(navigator.browserLanguage || navigator.language).toLowerCase() 
	}

	/*
	* 去重字符串两边空格
	*/
	function trim(str) {
		regExp1 = /^ */;
		regExp2 = / *$/;
		return str.replace(regExp1, '').replace(regExp2, '');
	}

	/*
	* 验证MAC
	*/
	function checkMac(mac){
		mac = mac.toUpperCase();
		if(mac == "" || mac.indexOf(":") == -1){
			return false;
		}else{
			var macs = mac.split(":");
			if(macs.length != 6){
				return false;
			}
			for(var i=0; i<macs.length; i++){
				if(macs[i].length != 2){
					return false;
				}
			}
			var reg_name=/[A-F\d]{2}:[A-F\d]{2}:[A-F\d]{2}:[A-F\d]{2}:[A-F\d]{2}:[A-F\d]{2}/; 
			if(!reg_name.test(mac)){ 
				return false; 
			} 
		}
		return true; 	
	}

	/*
	 * 获取URL参数值
	 */
	function getQueryString(name) {  
		var reg = new RegExp("(^|&)" + name + "=([^&]*)(&|$)");
		var r = window.location.search.substr(1).match(reg);
		if (r != null) {
			return unescape(r[2]);
		}  
		return null;
	}
	
	/*
	 * 检查IP是否合法
	 */
	function checkIP(ip){ 
		obj=ip; 
		var exp=/^(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$/; 
		var reg = obj.match(exp); 
		if(reg==null){ 
			return false;
		}else{ 
			return true;
		} 
	} 
	/*
	 * 检查掩码是否合法
	 */
	function checkMask(mask){ 
		obj=mask; 
		var exp=/^(254|252|248|240|224|192|128|0)\.0\.0\.0|255\.(254|252|248|240|224|192|128|0)\.0\.0|255\.255\.(254|252|248|240|224|192|128|0)\.0|255\.255\.255\.(254|252|248|240|224|192|128|0)$/; 
		var reg = obj.match(exp); 
		if(reg==null){ 
			return false;
		}else{ 
			return true;
		} 
	}

	/*
	 * 功能：实现IP地址，子网掩码，网关的规则验证
	 * 参数：IP地址，子网掩码，网关
	 * 返回值：BOOL
	 */
	var validateNetwork = function(ip,netmask,gateway){
		var parseIp = function(ip){
			return ip.split(".");
		}
		var conv = function(num){
			var num = parseInt(num).toString(2);
			while((8-num.length) > 0)num = "0"+num;
			return num;
		}
		var bitOpera = function(ip1,ip2){
			var result = '',binaryIp1 = '',binaryIp2 = '';
			for(var i = 0; i < 4; i++){
				if(i != 0)result += ".";
				for(var j = 0; j < 8; j++){
					result += conv(parseIp(ip1)[i]).substr(j,1) & conv(parseIp(ip2)[i]).substr(j,1)
				}
			}
			return result;
		}
		var ip_re = /^(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9])\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[0-9])$/;
		if (ip == null || netmask == null || gateway == null) {
			return false;
		}
		if (!ip_re.test(ip) || !ip_re.test(netmask) || !ip_re.test(gateway)) {
			return false
		}
		return bitOpera(ip,netmask) == bitOpera(netmask,gateway);
	}

	/*
	* 获取错误码
	*/
	function getErrorCode(type){
		switch(type){
			case 19 : return "宽带账号/密码错误！"; break;
			case 20 : return "服务器无响应！"; break;
			default : return "其它错误："+type;
		}
	}

	var router = {
		//获取系统版本
		getVersion : function(){
			$.ajax({
				 type: "POST",
				 url: actionUrl,
				 data: "fname=system&opt=firmstatus&function=get&flag=local",
				 dataType: "json",
				 success: function(data){
					var jsonObject = data;   
					if(jsonObject.error == 0){
						$("#version").html(jsonObject.localfirm_version);
					}else{
						$("#version").html("获取系统版本失败！");
					}
				 }
			 });
		},
		//获取网络类型
		getWanInfo : function(){
			$.ajax({
				 type: "POST",
				 url: actionUrl,
				 data: "fname=net&opt=wan_info&function=get",
				 dataType: "json",
				 success: function(data){
					var jsonObject = data;   
					if(jsonObject.error == 0){
						$("#netType").html(getWanNetType(jsonObject.mode));
						$("#wanIP").html(jsonObject.ip);
					}else{
						$("#netType").html("获取网络类型失败！");
						$("#wanIP").html("获取外网IP失败！");
					}
				 }
			 });
		},
		//获取model.js
		getModel : function(){
			$.getScript('http://static.wiair.com/conf/model.js', function() {
				MODEL = MODEL;
			});
		},
		//获取LAN IP
		getLanIP : function(){
			$.ajax({
				 type: "POST",	
				 url: actionUrl,
				 data: "fname=net&opt=dhcpd&function=get",
				 dataType: "json",
				 success: function(data){
					var jsonObject = data;   
					if(jsonObject.error == 0){
						$("#lanIP").html(jsonObject.ip);
					}else{
						$("#lanIP").html("获取本机IP失败！");
					}
				 }
			 });
		},
		//获取路由器动态信息
		getDynamicInfo : function(){
			$.ajax({
				 type: "POST",
				 url: actionUrl,
				 data: "fname=system&opt=main_page&function=get",
				 dataType: "json",
				 success: function(data){
					router.getWanInfo();
					var jsonObject = data; 
					if(jsonObject.error == 0){
						if(jsonObject.connected == true){
							document.getElementById("netStatus").innerHTML = "已连接";
						}else{
							document.getElementById("netStatus").innerHTML = "未连接&nbsp;&nbsp;<input onclick=\"javascript:document.location='admin.html?c=1';\" type=\"button\" value=\"连接\">";
						}
						$("#curSpeed").html(jsonObject.cur_speed + "KB/S");
						$("#totalSpeed").html((jsonObject.total_speed / 1024).toFixed(0) + "M");
						var devices = "";
						var terminals = jsonObject.terminals;
						if(terminals != null && terminals.length > 0){
							for(var i=0; i<terminals.length; i++){
								if(terminals[i].connected == true){
									var model = "";
									if(MODEL != null){
										if(MODEL[terminals[i].vendor] === undefined){
										}else{
											model = MODEL[terminals[i].vendor] + " - ";
										}
									}
									var name = "<b>" + model + decodeURIComponent(terminals[i].name) + "</b>";
									var used_time = "时长："+(terminals[i].used_time / 3600).toFixed(2) + "小时";
									devices += name+"<br/>"+used_time+"<br/>";
								}
							}
						}
						$("#devices").html(devices);
					 }else{
						$("#curSpeed").html("--");
						$("#totalSpeed").html("--");
					 }
				 }
			 });
		}
	}

	//获取网络类型字符串
	function getWanNetType(type){
		switch(type){
			case 1 : return "动态IP"; break;
			case 2 : return "PPPOE"; break;
			case 3 : return "静态IP"; break;
			default : return "获取网络类型失败";
		}
	}

	//获取Router Info
	function init(){
		router.getModel();
		router.getVersion();
		router.getLanIP();
		setInterval(function(){
			router.getDynamicInfo();
		},commonInterval);
	}