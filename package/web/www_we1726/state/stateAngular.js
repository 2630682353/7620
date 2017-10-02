/**
 * Created by lad on 2016/5/31.
 */

/*获取设备状态信息*/
app.controller('stateCtrl', function ($scope, $http) {
    $scope.hideMainWifi = false;
    $scope.wanErr = true;
    $scope.wifiClients = 0;
    //$scope.systeminfo = getSysinfo();  //旧版 现已迁移至footerCtrl控制器下
    urlHref($scope);
    $scope.deviceHref = 'device.html';
    GetWifiState();
    GetWanState();
    $scope.winfo = getWanInfo();
    if($scope.winfo.connected == 0){
        $scope.wanErr = false;
    }

    /*查询internet连接状态*/
    //window.setInterval(testNet, 5000);
    $http.post(actionUrl + 'fname=system&opt=firmstatus&function=get')
        .success(function (data) {
			var portObj = data.port_status//00000
	        $scope.showport1 = portObj.charAt(0)==1?true:false;
	        $scope.showport2 = portObj.charAt(1)==1?true:false;
	        $scope.showport3 = portObj.charAt(2)==1?true:false;
	        $scope.showport4 = portObj.charAt(3)==1?true:false;
	        $scope.showport5 = portObj.charAt(4)==1?true:false;        
        })


    /*查询终端设备数*/
    $http.post(actionUrl + 'fname=system&opt=main&function=get')
            .success(function (data) {
                if (data.error == 0) {
                    var j = 0;
                    for (var i = 0; i < data.terminals.length; i++) {
                        var flag = data.terminals[i].flag;
                        if (flag.charCodeAt(1) == 84) {	//T
                            j++;
                        }
                    }
                    $scope.wifiClients = j;
                }
            })
    /*无线中继扫描*/
    $scope.toggleWisp = function () {
        console.log('toggleWisp');
        //$http.post('/cgi-bin/webapi?action=wisp_scan',postToken())
        //    .success(function(data){
        //        if (data.resp.err == 0) {
        //            $scope.scanWifi = data.content;
        //        }
        //    });
    }

    /*修改登录密码*/
    $scope.toggleChangePasswd = function () {
        //console.log('toggleChangePasswd');
        showSetDialog('routerSetPwd');
    }


    /*设置wifi*/
    $scope.toggleSetWifi = function () {
        //console.log('toggleSetWifi');
        GetWifiState();
        GetWifiGeuestState();
        showSetDialog('routerSetWifi');
    }
    /*内网设置(Lan)*/
    $scope.toggleSetLan = function () {
        //console.log('toggleSetLan');
        GetLanState();
        showSetDialog('routerSetLan');
    }

    /*设备重启*/
    $scope.toggleReboot = function () {
        //console.log('toggleReboot');
        showSetDialog('routerReboot');
    }
    /*执行设备重启命令*/
    $scope.submitReboot = function () {
        //console.log('submitReboot');
        $http.post(actionUrl + 'fname=system&opt=setting&action=reboot&function=set');
        CheckWebActive($http,'','正在重启路由器。。。与路由器的连接可能会断开！');
    }
    /*外网设置（WAN）*/
    $scope.toggleSetWan = function () {
        //console.log('toggleSetWan');
        GetWanState();
        showSetDialog('routerSetInter');
    }
    /*Wisp 连接测试*/
    $scope.toggleTestWisp = function () {
        console.log('toggleTestWisp');
    }
    /*获取wifi 信息*/
    function GetWifiState() {
        $scope.wifiName = '';
        $http.post(actionUrl + 'fname=net&opt=wifi_ap&function=get')
                .success(function (data) {
                    if (data.error == 0) {
                        $scope.wifiName = data.ssid;
                        var wifiInfo = data;
                        $scope.$broadcast('wifiInfo', wifiInfo);
                    } else {
                        var tipText = 'err:' + data.error;
                        tipModel(tipText);
                    }
                })
                .error(function () {
                    var tipText = '网络错误，请刷新重试！';
                    tipModel(tipText);
                });
    }

    function GetWifiGeuestState() {
        $scope.guestWifiSSID = '';
        $http.post(actionUrl + 'fname=net&opt=wifi_vap&function=get')
                .success(function (data) {
                    if (data.error == 0) {
                        $scope.guestWifiSSID = data.vssid;
                        $scope.guestWifiPasswd = data.password;
                        $scope.guestWifiSwitch = data.enable;
                        var guestwifiInfo = data;
                        $scope.$broadcast('guestwifiInfo', guestwifiInfo);
                    } else {
                        var tipText = 'err:' + data.error;
                        tipModel(tipText);
                    }
                })
                .error(function () {
                    var tipText = '网络错误，请刷新重试！';
                    tipModel(tipText);
                });
    }

    /*获取当前lan状态*/
    function GetLanState() {
        var lanInfo = {
            'lanIp1': 192,
            'lanIp2': 168,
            'lanIp3': 1,
            'lanIp4': 1,
            'dhcpSwitch': 1,
            'dhcpIp': 100,
            'dhcpLimit': 100,
            'leasetime': 12,
            'netmask': '255.255.255.0',
        };
        $http.post(actionUrl + 'fname=net&opt=dhcpd&function=get')
                .success(function (ret) {
                    var tipText;
                    if (ret.error == 0) {
                        var ipaddr = ret.ip;
                        lanInfo.netmask = ret.mask;
                        lanInfo.leasetime = ret.time;
                        var ips = ipaddr.split('.');
                        lanInfo.lanIp1 = ips[0];
                        lanInfo.lanIp2 = ips[1];
                        lanInfo.lanIp3 = ips[2];
                        lanInfo.lanIp4 = ips[3];
                        lanInfo.dhcpSwitch = ret.enable;

                        //if (lanInfo.dhcpSwitch == 1) {  //关闭状态也可获取 20160918
                        
                            lanInfo.dhcpIp = parseInt(ret.start.split('.')[3]);
                            var end = ret.end;
                            lanInfo.dhcpLimit = parseInt(end.split('.')[3] - lanInfo.dhcpIp);
                            //lanInfo.leasetime = ret.content.dhcp_leasetime;
                        
                        //}

                        //广播给所有子controller
                        $scope.$broadcast('lanInfo', lanInfo);
                    } else if (ret.error == 2) {
                        tipText = '登录错误，请重新登录';
                        tipModel(tipText, 'login.html');
                    } else {
                        tipText = 'err:' + ret.resp.msg;
                        tipModel(tipText);
                    }
                })
                .error(function () {
                    window.location.reload(true);
                });
    }

    /*获取当前wan信息*/
    function GetWanState() {
        $http.post(actionUrl + 'fname=net&opt=wan_info&function=get')
            .success(function (ret) {
                if (ret.error == 0) {
                    $scope.$broadcast('wanInfo', ret);
                    $scope.ipAddr = $scope.winfo.ip;
                    switch (ret.mode) {
                        case 1:
                            $scope.wanProto = '自动方式(DHCP)';
                            break;
                        case 3:
                            $scope.wanProto = '手动方式(静态IP)';
                            break;
                        case 2:
                            $scope.wanProto = '宽带拨号(PPPOE)';
                            break;
                        default :
                            $scope.wanProto = '';

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

})



