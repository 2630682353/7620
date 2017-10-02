/**
 * Created by lad on 2016/5/31.
 */


app.controller('lanCtrl', function ($scope, $http) {
    $scope.lanIp1 = 192;
    $scope.lanIp2 = 168;
    $scope.lanIp3 = 1;
    $scope.lanIp4 = 1;
    $scope.dhcpIp = 100;
    $scope.dhcpSwitch = 1;
    $scope.dhcpLimit = 100;
    $scope.netmask = '255.255.255.0';
    $scope.leasetime = 12;
    $scope.leasetimes = [
        1, 2, 6,
        12, 24, 48,
    ];
    //注册事件监听，更新数据
    $scope.$on('lanInfo', function (event, info) {
        $scope.lanIp1 = info.lanIp1;
        $scope.lanIp2 = info.lanIp2;
        $scope.lanIp3 = info.lanIp3;
        $scope.lanIp4 = info.lanIp4;
        $scope.dhcpIp = info.dhcpIp;
        $scope.dhcpLimit = info.dhcpLimit;
        $scope.netmask = info.netmask;
        $scope.dhcpSwitch = info.dhcpSwitch;
        $scope.leasetime = info.leasetime;
        //$scope.leasetime = "text";
        $scope.btSave = false;
    });

    /*ip地址合法性检查*/
    $scope.lanIpCheck = function (index) {
        switch (index) {
            case 3:
                if ($scope.lanIp3 < 0 || $scope.lanIp3 > 255) {
                    $scope.lanTipIf = true;
                    $scope.lanTipInfo = '请输入0~255之间的任意数值！';
                } else {
                    $scope.lanTipIf = false;
                }
                break;
            case 4:
                if ($scope.lanIp4 <= 0 || $scope.lanIp4 > 255) {
                    $scope.lanTipIf = true;
                    $scope.lanTipInfo = '请输入1~255之间的任意数值！';
                } else {
                    $scope.lanTipIf = false;
                }
                break;
            case 5:
                if ($scope.dhcpIp <= 0 || (parseInt($scope.dhcpIp) + parseInt($scope.dhcpLimit)) > 255) {
                    $scope.lanTipDhcpIf = true;
                    $scope.lanTipDhcpInfo = '超出范围：请输入1~' + (255 - $scope.dhcpLimit) + '之间的任意数值或修改限制数！';
                } else {
                    $scope.lanTipDhcpIf = false;
                }
                break;
            default :
                var tipText = '错误的位置检查，请联系开发人员！' + index;
                tipModel(tipText);
                break;
        }
    }

    $scope.submitLan = function () {
        var tipText;
        if ($scope.lanIp3 < 0 || $scope.lanIp3 > 255) {
            $scope.lanTipInfo = '局域网IP第三位非法：请输入0~255之间的任意数值!';
            return;
        }
        if ($scope.lanIp4 <= 0 || $scope.lanIp4 > 255) {
            $scope.lanTipInfo = '局域网IP第四位非法：请输入1~255之间的任意数值！';
            return;
        }
        if ($scope.dhcpIp <= 0 || (parseInt($scope.dhcpIp) + parseInt($scope.dhcpLimit)) > 255) {
            $scope.lanTipDhcpIf = true;
            $scope.lanTipDhcpInfo = 'DHCP服务起始IP超出范围：请输入1~' + (255 - $scope.dhcpLimit) + '之间的任意数值或修改限制数！';
            return;
        }
        if ($scope.dhcpLimit < 0) {
            $scope.lanTipNumIf = true;
            $scope.lanTipNumInfo = 'DHCP服务限制非法：请输入0~150之间的任意数值！';
            return;
        }

        $scope.lanTipDhcpIf = false;
        $scope.lanTipNumIf = false;
        var postJson = {
            'content': {
                'ipaddr': $scope.lanIp1 + '.' + $scope.lanIp2 + '.' + $scope.lanIp3 + '.' + $scope.lanIp4,
                'netmask': $scope.netmask,
                'dhcp_ignore': $scope.dhcpSwitch,
                'dhcp_start': $scope.lanIp1 + '.' + $scope.lanIp2 + '.' + $scope.lanIp3 + '.' + $scope.dhcpIp,
                'dhcp_limit': $scope.lanIp1 + '.' + $scope.lanIp2 + '.' + $scope.lanIp3 + '.' + (parseInt($scope.dhcpLimit) + parseInt($scope.dhcpIp)),
                'dhcp_leasetime': $scope.leasetime,
            }
        };

        //var data = 'fname=net&opt=dhcpd&function=set&start=' + postJson.content.dhcp_start + '&end=' + postJson.content.dhcp_limit + '&mask=' + postJson.content.netmask + '&ip=' + postJson.content.ipaddr +'&enable=' + postJson.content.dhcp_ignore;
        var data = 'fname=net&opt=dhcpd&function=set&start=' + postJson.content.dhcp_start + '&end=' + postJson.content.dhcp_limit + '&mask=' + postJson.content.netmask + '&ip=' + postJson.content.ipaddr + "&time=" +  postJson.content.dhcp_leasetime +'&enable=' + postJson.content.dhcp_ignore;
        $http.post(actionUrl + data, postJson);
        CheckWebActive($http, null);


    }
    /*dhcp开关*/
    $scope.toggleDhcpSwitch = function () {
        $scope.dhcpSwitch ^= 1;
        //console.log('toggleDhcpSwitch', $scope.dhcpSwitch);
    }
});
