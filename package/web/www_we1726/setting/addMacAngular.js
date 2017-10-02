/**
 * Created by lad on 2016/6/3.
 */
app.controller('addMacCtrl', function ($scope, $http) {

    $scope.addMacs = new Array();
    $http.post(actionUrl + 'fname=net&opt=acl_white&function=get&ifindex=0')
            .success(function (ret) {
                if (ret.error == 0) {
                    SetFireWallContent(ret);
                    $scope.enalbeFirewall = parseInt(ret.enable);
                    var trustList = ret.mac;
                    for (var i = 0; i < trustList.length; i++) {
                        var item = trustList[i];
//                        if (ret.enable == 1) {
                            var addItem = {'mac': item};
                            $scope.addMacs.push(addItem);
//                        }
                    }
                }
            });
    /*增加新的白名单*/
    $scope.toggleAddMac = function () {
        var item = {
            'mac': '',
        };
        $scope.addMacs.push(item);
        if ($scope.addMacs.length >= 33) {
            tipModel('最多能添加32组MAC');
        }
    }


    /*全部控制访问开关*/
    $scope.setEnableFirewall = function () {
        $scope.enalbeFirewall ^= 1;
        EnableFireWall($scope.enalbeFirewall);
    }
    /*提交设置*/
    $scope.toggleSubmit = function () {
        for (var i = 0; i < $scope.addMacs.length; i++) {
            if (!IsValidMac($scope.addMacs[i].mac)) {
                if ($scope.addMacs[i].mac==""){
                    continue;
                }
                var tipText = '请检查输入的mac是否合法！';
                tipModel(tipText);
                return;
            }
        }


        for(var i = 0;i<$scope.addMacs.length;i++){
            
            if($scope.addMacs[i].mac==""){
                continue;
            }

            for(var j = 0;j<$scope.addMacs.length;j++){
    
                if (i!=j&&$scope.addMacs[i].mac==$scope.addMacs[j].mac){
                    tipText = 'mac地址重复！';
                    tipModel(tipText);
                    return;
                }
    
            }
        
        }



        showLoading(null);
        var enable = GetFireWallEnable();
        /*打开白名单则关闭qos*/
//        if (GetFireWallEnable() == 1) {
        var postQos = {
            "content": new Array(),
        }
        var macL = $scope.addMacs.length, macList = [], list = '';
        for (var i = 0; i < $scope.addMacs.length; i++) {
            if($scope.addMacs[i].mac != ""){
                macList.push($scope.addMacs[i].mac);
            }else{
                macL--;
                continue;
            }
        }
        list = macList.join(';');
        var dat = 'fname=net&opt=acl_white&function=set&ifindex=0&mac=' + list + '&mac_num=' + macL + '&enable=' + enable;
        $http.post(actionUrl + dat, postQos)
                .success(function (data) {
                    showRet(data);
                })
                .error(function (ret) {
                    showRet(null);
                });
//        }
    }

    $scope.checkMac = function (index) {
        //console.log('checkMac:index:', index, ', obj=', $scope.addMacs[index]);
        var mac = $scope.addMacs[index].mac;


        if (IsValidMac(mac)) {

            for(var i = 0;i<$scope.addMacs.length;i++){
                if (i!=index&&mac==$scope.addMacs[i].mac){
                    tipText = 'mac地址重复！';
                    tipModel(tipText);
                    return;
                }
            }

            AddWhiteName(mac, 1);
        
        } else {
            
            tipText = '请输入合法的mac地址：例如：2C:C6:55:01:69:28';

            tipModel(tipText);
        }

    }
    $scope.delMac = function (index) {
        //console.log('delMac:index:', index, ', obj=', $scope.addMacs[index]);
        AddWhiteName($scope.addMacs[index].mac, 0);
        $scope.addMacs.splice(index, 1);
    }

});
