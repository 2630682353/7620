/**
 * Created by lad on 2016/6/1.
 */

/*终端设备*/
app.controller('deviceCtrl', function ($scope, $http) {
    $scope.qosSwitch = 0;
    GetClientsInfo();
    urlHref($scope);
    //$scope.systeminfo = getSysinfo();  //旧版 现已迁移至footerCtrl控制器下
    /*切换不同的设置页面*/
    $scope.deviceSetting = function (index) {

        $('.devicetab .bd > ul').removeClass('on');
        $('.devicetab .bd > ul:eq(' + index + ')').addClass('on');
        switch (index) {
            case 0:
                GetClientsInfo();
                break;
            case 1:
                GetClientsQosInfo();
                break;
            default :
                alert('不存在当前的页面，请联系开发人员！');
        }
    }
    /*防火墙*/
    $scope.fswitchFire = function (client) {
        client.checked ^= 1;
        EnableFireWall(1);
        AddWhiteName(client.mac, client.checked);
        var act = 'off';
        if (client.checked == 0) {
            act = 'on';
        }
        var data = 'fname=net&opt=host_black&function=set&mac=' + client.mac + '&act=' + act;
        $http.post(actionUrl + data)
                .success(function (ret) {
                    showRet(ret);
                })
                .error(function () {
                    showRet(null);
                });
    }

    /*qos限速开关*/
    $scope.toggleQosSwitch = function () {
        $scope.qosSwitch ^= 1;
//        for (var i = 0; i < $scope.nbvqos.length; i++) {
//            var item = $scope.nbvqos[i];
//            item.enable = $scope.qosSwitch;
//        }
    }

    /*保存限速*/
    //old 20161027
    // $scope.qosSubmit_ = function () {
    //     var info = $(this)[0].qos,tipText= '数值不合法！';
    //     //showLoading(null);
    //     /*如果打开限速则关闭白名单*/
    //     var dat, speed, upspeed;
        
    //     if (isNaN(info.ratedl)||isNaN(info.rateup)||info.ratedl<0||info.rateup<0){
    //         tipText="数值不合法！";
    //         tipModel(tipText, location);
    //         return;
    //     }

    //     if (info.ratedl>999||info.rateup>999) {
    //         tipText="下载与上传速度限制999kbit以内！";
    //         tipModel(tipText, location);
    //         return;
    //     }
    //     speed = parseInt(info.ratedl * 1000 / 8);
    //     upspeed = parseInt(info.rateup * 1000 / 8);
    //     dat = 'fname=net&opt=host_ls&function=set&mac=' + info.mac + '&speed=' + speed + '&up_speed=' + upspeed;

    //     $http.post(actionUrl + dat)
    //         .success(function (data) {
    //             if (data.error == 0) {
    //                 showRet(data);
    //             } else {
    //                 showRet(data.error);
    //             }
    //         })
    //         .error(function (ret) {
    //             showRet(null);
    //         });
    // }

    //新保存按钮20161027
    $scope.qosSubmit = function () {
        showLoading("正在保存设置。。。");
        var info=$scope.nbvqos;
        for (var i = 0,len=info.length; i < len; i++) {
            var ratedl=info[i].ratedl;
            var rateup=info[i].rateup;
            if(isNaN(ratedl)||isNaN(rateup)||ratedl<0||rateup<0||ratedl>999||rateup>999||encodeURI(ratedl).indexOf('%20')!='-1'||encodeURI(rateup).indexOf('%20')!='-1'||encodeURI(ratedl).indexOf('.')!='-1'||encodeURI(rateup).indexOf('.')!='-1'){
                tipModel("设置参数不合法，请检查并重试！", false);
                return;
            }
            setnbvqos(info[i].mac,ratedl,rateup);
        }
        setTimeout(function(){
            closeLoading();
            tipModel('设置成功！', location);
        },5000)  
    }
    
    $scope.reset=function(){
        var info=$scope.nbvqos;
        for (var i = 0,len=info.length; i < len; i++) {
            $scope.nbvqos[i].ratedl=0;
            $scope.nbvqos[i].rateup=0;
        }
    }

    function setnbvqos(mac,speed,upspeed){
        speed = parseInt(speed * 1000 / 8);
        upspeed = parseInt(upspeed * 1000 / 8);
        dat = 'fname=net&opt=host_ls&function=set&mac=' + mac + '&speed=' + speed + '&up_speed=' + upspeed;
        $http.post(actionUrl + dat)
            .success(function (data) {
                if (data.error == 0) {
                    //
                } else {
                    //
                }
            })
            .error(function (ret) {
                //
            });
    }
    /*获取当前连接客户端信息*/
    function GetClientsInfo() {
        $("#vistSetting").addClass('on').siblings().removeClass('on');
        $http.post(actionUrl + 'fname=net&opt=main&function=get')
                .success(function (data) {
                    if (data.error == 0) {
                        var idx;
                        $scope.clients = data.terminals;

                        for (var i = 0; i < $scope.clients.length; i++)
                        {
                            if ($scope.clients[i].ip == '') {
                                $scope.clients[i].ip = '离线';
                            }
                        }
                        GetWhiteNameList();
                    } else if (data.error == '10007') {
                        // alert("登录错误，请重新登录");
                        // window.location.href = 'login.html';
                        var tipText = '登录错误，请重新登录';
                        tipModel(tipText, 'login.html');
                    } else {
                        alert('err:' + data.resp.msg);
                    }
                })
    }
    /*获取设置的客户端的qos信息*/
    function GetClientsQosInfo() {
        $("#vistSetting").removeClass('on').siblings().addClass('on');
        $scope.nbvqos = new Array();
        $http.post(actionUrl + 'fname=system&opt=host_if&function=get&if=FFFFFFFT')
                .success(function (data) {
                    if (data.error == 0) {
                        var sclients = $scope.clients;
                        var clients = data.terminals;
                        var qoses = 1;
                        for (var i = 0; i < clients.length; i++) {
                            var qosItem = {};
                            qosItem.name = sclients[i].name;
                            if (sclients[i].ip) {
                                qosItem.ip = sclients[i].ip;
                            } else {
                                qosItem.ip = '离线';
                            }
                            qosItem.ratedl = clients[i].ls * 8 / 1000;
                            qosItem.rateup = clients[i].ls_up * 8 / 1000;
                            qosItem.mac = sclients[i].mac;
//                            qosItem.up_bytes = sclients[i].up_speed;
//                            qosItem.down_bytes = sclients[i].down_bytes;
//                            if (qosItem.up_bytes > 1024) {
//                                qosItem.up_bytes = (qosItem.up_bytes / 1024).toFixed(2) + 'KB/s';
//                            } 
//                            if (terminals[i].up_speed > 1024) {
//                                terminals[i].up_speed = (terminals[i].up_speed / 1024).toFixed(2) + 'MB/S';
//                            } else {
//                                terminals[i].up_speed = terminals[i].up_speed + 'KB/S';
//                            }                            
                            qosItem.enable = 0;
                            for (var j = 0; j < qoses.length; j++) {
                                if (qoses[j].mac == qosItem.mac
                                        && qoses[j].ip == qosItem.ip
                                        && qoses[j].name == qosItem.name
                                        ) {
                                    qosItem.ratedl = qoses[j].ratedl;
                                    qosItem.rateup = qoses[j].rateup;
                                }
                            }
                            $scope.nbvqos.push(qosItem);

                        }
                        $scope.qosSwitch = 1;
                    } else if (data.resp.err == 2) {
                        var tipText = '登录错误，请重新登录';
                        tipModel(tipText, 'login.html');
                    } else {
                        alert('err:' + data.resp.msg);
                    }
                });
    }
    /*获取黑名单信息*/
    function GetWhiteNameList() {
        var clients = $scope.clients;
        for (var i = 0; i < clients.length; i++) {
            if (clients[i].flag.substr(2, 1) == 'F') {
                clients[i].checked = 1;
            } else {
                clients[i].checked = 0;
            }
        }
    }

    /*检查限速输入值是否有效*/
    $scope.checkBit = function (index, qos) {
        switch (index) {
            case 0://下载
                if (isNaN(qos.ratedl)||qos.ratedl < 0||qos.ratedl > 999||encodeURI(qos.ratedl).indexOf('%20')!='-1'||encodeURI(qos.ratedl).indexOf('.')!='-1') {
                    tipModel("错误的下载速率：请输入合法的数值！", false);
                }
                break;
            case 1://上传
                if (isNaN(qos.rateup)||qos.rateup < 0||qos.rateup > 999||encodeURI(qos.rateup).indexOf('%20')!='-1'||encodeURI(qos.rateup).indexOf('.')!='-1') {
                    tipModel("错误的下载速率：请输入合法的数值！", false);
                }
                break;
            default :
                tipModel("参数错误，请联系开发人员！", false);
                break;
        }
    }

})

