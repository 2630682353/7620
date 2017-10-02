/**
 * Created by lad on 2016/6/1.
 */

app.controller('wanDialogCtrl', function ($scope, $http) {
    $scope.wanmodels = [
        {'des': 'dhcp', 'text': '客户端模式'},
        //{'des':'wisp', 'text':'中继模式'},
    ];
    $scope.autoChecked = 1;
    $scope.wanMode = $scope.wanmodels[0];
    $scope.pppoeMsg="宽带拨号未启用...";
    $scope.mode="";
    
    GetpppoeState();
    GetwanState();



    $scope.$on('wanInfo', function (event, wanInfo) {
    	$scope.wanIP = wanInfo.ip;
        $scope.ipManual = wanInfo.ip;
        $scope.maskManual = wanInfo.mask;
        $scope.gatewayManual = wanInfo.gateway;
        $scope.dnsManual = wanInfo.DNS1;
        //$scope.dhcp_op = wanInfo.DNS1;
        //$scope.poeUser = wanInfo.user;
        //$scope.poePasswd = wanInfo.passwd;
        //alert($scope.gatewayManual==$scope.dhcp_op);
        //if($scope.gatewayManual==$scope.dhcp_op){
        //    $scope.dhcp_op="";
        //}
        switch (wanInfo.mode) {
            case 1:
                ShowTab(1);
                $scope.showAutoConn = 0;
                $scope.pwdHide = 0;
                $scope.myDns = wanInfo.dns;
                $scope.wanMode = $scope.wanmodels[0];
                $scope.autoChecked = 1;
                $scope.manualChecked = 0;
                break;
            case 3:
                ShowTab(1);
                $scope.showAutoConn = 1;
                $scope.pwdHide = 1;
                $scope.autoChecked = 0;
                $scope.manualChecked = 1;
                break;
            case 'pppoe':
            case 2:
                ShowTab(0);
                break;
            default :
                alert('无法识别的协议类型[' + wanInfo.mode + ']，请联系开发人员！');
                break;
        }

    });
    /*显示不同的分页*/
    $scope.showTab = function (index) {
        ShowTab(index);
    }
    /*显示不同的连接方式配置*/
    $scope.connMode = function (auto) {
        // $scope.autoChecked ^= 1;
        // $scope.manualChecked ^= 1;
        $scope.showAutoConn = auto;
        switch (auto) {
            case 0:
                $scope.autoChecked = true;
                $scope.manualChecked = false;
                //$scope.content.proto = 'dhcp';
                break;
            case 1:
                $scope.manualChecked = true;
                $scope.autoChecked = false;
                //$scope.content.proto = 'static';
                break;
        }
    }
    /*显示密码*/
    $scope.showPwd = function () {
        $scope.pwdHide ^= 1;
    }
    /*保存设置*/
    $scope.wanSubmit = function () {
        //console.log('wanSubmit');
        var sysinfo = $scope.systeminfo

        if ($('.interTab02').hasClass('on')) {
            
            if ($scope.autoChecked) {
                var dns = $scope.dnsChecked > 0 ? $scope.dhcp_op : "";
                data = "fname=net&opt=wan_conf&function=set&mode=1&mtu=1500&dns=" + dns + "&mac=" + sysinfo.mac_eth01;
            } else {
                if (!CheckManualSet()) {
                    //alert('请输入正确的ip地址！');
                    return;
                }
                data = "fname=net&opt=wan_conf&function=set&ip=" + $scope.ipManual + "&mask=" + $scope.maskManual + "&gw=" + $scope.gatewayManual + "&dns=" + $scope.dnsManual + "&dns1="+ $scope.dnsManual +"&mac=" + sysinfo.mac_eth01 + "&mode=3&mtu=1500";
                
            }

        } else {
            if (!CheckPPPOE()) {
                return;
            }
            var data = "fname=net&opt=wan_conf&function=set&user=" + $scope.poeUser + "&passwd=" + $scope.poePasswd + "&mode=2&mtu=1480&mac=" + sysinfo.mac_eth01;
        }
        $http.post(actionUrl + data + "&math=" + Math.random());
        //alert(data);
        CheckPPPOEState($http, null);
    }
    /*显示自定义dns*/
    $scope.showMyDns = function () {
        $scope.showDnsFlag ^= 1;
        $scope.dnsChecked ^= 1;
    }
    /*显示不同的tab页面*/
    function ShowTab(index) {
        number = index;
        $('.routerslide .bd > ul').removeClass('on');
        $('.routerslide .bd > ul:eq(' + index + ')').addClass('on');
        //不同的分页，保存不同的协议
        switch (index) {
            case 0:
//                $scope.content.proto = 'pppoe';
                $('.interTab01').addClass('on').siblings().removeClass('on');

                break;
            case 1:
                if ($scope.autoChecked) {
//                    $scope.content.proto = 'dhcp';
                    $('.interTab02').addClass('on').siblings().removeClass('on');
                } else if ($scope.manualChecked) {
//                    $scope.content.proto = 'static';
                    $('.interTab02').addClass('on').siblings().removeClass('on');
                }
                break;
        }
    }
    function SetPostContent() {
        $scope.content.ipaddr = $scope.ipManual;
        $scope.content.netmask = $scope.maskManual;
        $scope.content.gateway = $scope.gatewayManual;
        $scope.content.dns = $scope.dnsManual;
        $scope.content.username = $scope.poeUser;
        $scope.content.password = $scope.poePasswd;
    }

    function CheckPPPOE() {
        if ($scope.poeUser == '' || $scope.poeUser == undefined) {
            tipModel_('请输入宽带账号!',"routerSetInter");
            return false;
        }
        if ($scope.poeUser.indexOf("\"") > -1 || $scope.poeUser.indexOf("'") > -1 || $scope.poeUser.indexOf("\\") > -1) {
            tipModel_("宽带账号不能包含 单双引号、反斜杠！","routerSetInter");
            return false;
        }

        if ($scope.poePasswd == '' || $scope.poePasswd == undefined) {
            tipModel_('请输入宽带密码',"routerSetInter");
            return false;
        }

        if ($scope.poePasswd.indexOf("\"") > -1 || $scope.poePasswd.indexOf("'") > -1 || $scope.poePasswd.indexOf("\\") > -1) {
            tipModel_("宽带密码不能包含 单双引号、反斜杠！","routerSetInter");
            return false;
        }
        return true;
    }


    function CheckManualSet() {
        var checkIps = [
            $scope.ipManual,
            $scope.maskManual,
            $scope.gatewayManual,
            $scope.dnsManual
        ];
        var ipsMsg = [
            "ip",
            "子网掩码",
            "网关",
            "DNS"
        ];
        var length = checkIps.length;
        for (var i = 0; i < length; i++) {
            if (angular.isUndefined(checkIps[i]) || checkIps[i] == "") {
                tipModel("请输入" + ipsMsg[i]);
                return false;
            }
            if (angular.isUndefined(checkIps[i]) || !IsValidIp(checkIps[i])) {
                tipModel_("请输入正确的" + ipsMsg[i],"routerSetInter");
                //tipModel("请输入正确的" + ipsMsg[i]);
                return false;
            }
        }

        //20160905  新增原有设置条件
        if (!validateNetwork(checkIps[0], checkIps[1], checkIps[2])) {
            tipModel("IP地址+子网掩码+网关设置错误！");
            return false;
        }

        if (validateNetwork(checkIps[0], checkIps[1])) {
            tipModel("IP地址设置错误！");
            return false;
        }



        return true;
    }


    //静态ip验证
	function validateNetwork(ip, netmask, gateway) {

	    var parseIp = function (ip) {

	        return ip.split(".");

	    }

	    var conv = function (num) {

	        var num = parseInt(num).toString(2);

	        while ((8 - num.length) > 0)

	            num = "0" + num;

	        return num;

	    }

	    var bitOpera = function (ip1, ip2) {

	        var result = '', binaryIp1 = '', binaryIp2 = '';

	        for (var i = 0; i < 4; i++) {

	            if (i != 0)

	                result += ".";

	            for (var j = 0; j < 8; j++) {

	                result += conv(parseIp(ip1)[i]).substr(j, 1) & conv(parseIp(ip2)[i]).substr(j, 1)

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

	    return bitOpera(ip, netmask) == bitOpera(netmask, gateway);

	}


    /*获取当前宽带账号密码*/
    function GetpppoeState() {
        $http.post(actionUrl + 'fname=net&opt=wan_conf&function=get')
            .success(function (ret) {
                if (ret.error == 0) {
                    $scope.poeUser=ret.user;
                    $scope.poePasswd=ret.passwd;
                    $scope.dhcp_op=ret.dns;
                    if (angular.isDefined($scope.dhcp_op) && $scope.dhcp_op!="") {
				        $scope.showDnsFlag = 1;
				        $scope.dnsChecked = 1;
				    }
                } else {
                    var tipText = 'err:' + ret.error;
                    tipModel(tipText);

                }
            })
            .error(function () {
                window.location.reload(true);
            });
    }

    /*获取ip状态、网线连接状态*/
    function GetwanState() {
        //var pppoeMsg=$("#pppoeMsg");
        $.ajax({
            type: "POST",
            url: actionUrl + 'fname=net&opt=wan_info&function=get',
            async: false,
            dataType: "json",
            success: function(ret){
                if (ret.error == 0) {
                    $scope.link=ret.link;                 //网线连接状态
                    $scope.connected=ret.connected;       //是否已连接
                    $scope.reason=ret.reason;             //网络状态信息
                    $scope.mode=ret.mode;                 //上网方式            
                
                    if ($scope.link==0) {
                        $scope.pppoeMsg="未检测到网线连接！";
                    }else if($scope.link==1&&$scope.mode==2){
                        if($scope.connected==1){
                            $scope.pppoeMsg="宽带拨号成功，网络已连接！";
                        }else{
                            $scope.pppoeMsg=errorMessage($scope.reason);
                        }
                    }
                
                } else {
                    var tipText = 'err:' + ret.error;
                    tipModel(tipText);
                }
            },
            error: function(){
                window.location.reload(true);
            }
        })
    }

    function errorMessage(code) {
	    switch (code) {
	        case 0:
	            return "宽带拨号失败！";
	            break;
	        case 19 :
	            return "宽带账号/密码错误！";
	            break;
	        case 20:
	            return "运营商无响应！";
	            break;
	        case 21:
	            return "获取IP失败！";
	            break;
	        case 22:
	            return "IP获取成功，无法上网！";
	            break;
	        default:
	            return "宽带拨号连接超时！";
	    }
	}

	/*检测宽带拨号返回状态+loading层*/

	function CheckPPPOEState($http, links) {
	    showLoading('保存设置中。。。与路由器的连接可能会断开！');
	    if($('.interTab02').hasClass('on')){
		
		    var check = window.setInterval(testInter, 5000);
		    var delay = 0;
		    //var isSuccess = false;
		    function testInter() {
		        if (delay++ > 2) {
		            window.clearInterval(check);
		            closeLoading();
		            closeSetDialog();
		            var tipText = '设置成功，请检查是否连接到路由器，点击确定后，将自动重新检测！';
		            if (links) {
		                tipModel(tipText, links);
		            } else {
		                tipModel(tipText, location);
		            }
		        }
		    }
		
		}else{
		    // setTimeout(function(){
		    // 	GetwanState();
	     //        closeLoading();
	     //        closeSetDialog();
		    // 	tipModel($scope.pppoeMsg,location);
		    // },8000)
			var check_pppoe = window.setInterval(testInter_pppoe, 1000);
			var delay_pppoe = 0;
		    function testInter_pppoe() {
		    	GetwanState();
		        if ($scope.connected==1||delay_pppoe++ > 10) {
		            window.clearInterval(check_pppoe);
		            closeLoading();
		            closeSetDialog();
		            tipModel($scope.pppoeMsg,location);
		        }
		    }
		}
	
	}

    //tipModel_  tip层(改进版)
    function tipModel_(tipModel,contentBox) {

        showSetDialog('tipModel');

        $("."+contentBox).css({
            "visibility":"hidden",
            "height":0
        });

        $('#tipModelId').text(tipModel);

        $("#submitTip").click(function () {

            $("."+contentBox).css({
                "visibility":"visible",
                "height":"auto"
            });

            $('.tipModel').hide();

        });
    }


    //ip 网关 关联
    $scope.$watch('ipManual', function(newValue,oldValue){
        if(newValue==""&&oldValue=="")return;
        var ipM=getIPArray(newValue);
        var gtyM=getIPArray($scope.gatewayManual);
        //$scope.dnsManual=newValue;
        $scope.gatewayManual=ipM[0]+"."+ipM[1]+"."+ipM[2]+"."+gtyM[3];
    });

    $scope.$watch('gatewayManual', function(newValue,oldValue){
        if(newValue==""&&oldValue=="")return;
        var gtyM=getIPArray(newValue);
        var ipM=getIPArray($scope.ipManual);
        $scope.ipManual=gtyM[0]+"."+gtyM[1]+"."+gtyM[2]+"."+ipM[3];
    });

    /*IP转换为数组 ["192","168","10","1"] 模式*/
    function getIPArray(ip){
        return ip.split(/\./,4);
    }
});
