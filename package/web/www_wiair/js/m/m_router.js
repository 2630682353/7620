/* router.js */
$(function () {


    //添加隐藏ssid
//    $("#ssid_add").live("click", function () {
//        layer.open({
//            type: 1,
//            shadeClose: false,
//            content: '<div class="wifiConnectLayer" id="ssid_connect" style="display:block;"><div class="con"><p>添加隐藏网络</p><ul class="list"><li><span class="sp-l">网络SSID</span><span class="sp-r"><label for="ssid">请输入网络SSID</label><input type="text" name="ssid" class="inpt" id="ssid" autocomplete="off"></span></li><li><span class="sp-l">密码</span><span class="sp-r"><label for="ssid_pwd">请输入密码</label><input type="password" name="ssid_pwd" class="inpt" id="ssid_pwd" autocomplete="off"></span></li></ul><div class="box"><a href="javascript:;" class="btn layer-btn cannel">取消</a>&nbsp;&nbsp;<a href="javascript:;" class="btn layer-btn" id="ssid_confirm">确认</a></div></div></div>',
//            style: 'width:90%;background-color: transparent;  box-shadow: none;'
//        });
//    })

    //确认 添加隐藏ssid
//    $("#ssid_confirm").live("click", function () {
//        if ($("#ssid").val() == '') {
//            tips("请输入网络SSID");
//            return;
//        }
//        if ($("#ssid_pwd").val() == '') {
//            tips("请输入密码");
//            return;
//        }
//        layer.closeAll();
//        tips("添加成功!");
//    })
    /*************** internetType end**********************/




    /*************** getAccountInfo **********************/
    //获取宽带帐号密码
//    $("#goGet").click(function () {
//        layer.open({
//            type: 1,
//            shadeClose: false,
//            content: $("#accountLayer").html(),
//            style: 'width:90%;background-color: transparent;  box-shadow: none;'
//        });
//    })

    //获取按钮
//    $("#getAccount").live("click", function () {
//        $(".dot").hide();
//        $(".txt").removeClass("hide").text("路由器正在获取中...");
//        $(".d2").removeClass("error");
//        $(this).val("获取中...")
//        $(this).attr("disabled", "disabled");
//        var sec = 3;
//        var interval = window.setInterval(function () {
//            sec--;
//            if (sec == 0) {
//                window.clearInterval(interval);
//
//                /*获取失败
//                 $(".dot").show();
//                 $(".d2").addClass("error");
//                 $(".txt").text("获取失败!");
//                 $(".inptBtn").removeAttr("disabled");
//                 $(".inptBtn").val("重新获取");
//                 */
//
//
//                /* 获取成功*/
//                layer.closeAll();
//                layer.open({
//                    type: 1,
//                    shadeClose: false,
//                    content: $("#sucLayer").html(),
//                    style: 'width:90%;background-color: transparent;  box-shadow: none;'
//                });
//
//
//            } else {
//                $(".d1").delay(100).fadeIn();
//                $(".d2").delay(400).fadeIn();
//                $(".d3").delay(700).fadeIn();
//                setTimeout(function () {
//                    $(".dot").fadeOut();
//                }, 1200)
//            }
//
//        }, 2000)
//    })
    /*************** getAccountInfo end**********************/

    //无线设置
//    $("#wifi_btn").live("click", function () {
//        if ($("#wifi_acc").val() == '') {
//            tips("请输入用户名");
//            return;
//        }
//        if ($("#wifi_pwd").val() == '') {
//            tips("请输入密码");
//            return;
//        }
//        layer.closeAll();
//        layer.open({
//            title: "设置成功",
//            btn: ["确认"],
//            content: "密码设置成功后，需要您再重新连接无线WIFI。",
//            style: 'background-color:#056b9c;opacity:.75; box-shadow: none; text-align:left;color:#fff;'
//
//        });
//    })

//    $("#fun_list a").click(function () {
//        var id = $(this).attr("data-id");
//        //$("")
//    })

    //vpn设置

//    $(".tc-name-box a").click(function () {
//        $(this).addClass("selected").siblings("a").removeClass("selected");
//    })
//    $(".xf-name").live('click', function () {
//        var id = $(this).attr("data-id");
//        $("#" + id + "_box").removeClass("hide").siblings(".xf-name-box").addClass("hide");
//    });
//    $(".tc-name").click(function () {
//        var id = $(this).attr("data-id");
//        var desc = $(this).attr("data-desc");
//        $("#" + id + "_box").removeClass("hide").siblings(".tc-name-box").addClass("hide");
//        $(".tc-txt").text(desc);
//    })
//    $(".swichBtn").click(function () {
//        $(this).toggleClass("close-btn");
//    })

    /*********终端控制***********/
    //编辑名称
//    $(".edit").live("click", function () {
//        $(this).parent().find(".client-t").children("input").removeAttr("readonly").removeClass("nameInpt").select();
//    })
//    $(".client-t").find("input").live("focusout", function () {
//        $(this).attr("readonly", "readonly");
//        $(this).addClass("nameInpt");
//    })
    /*********终端控制 end***********/


    /*升级 重启  恢复出厂*/
//    $(".m-list a").click(function () {
//        layer.open({
//            type: 1,
//            content: '<div class="wifiConnectLayer" style="display:block;"><div class="con"><p>版本更新/重启/恢复出厂</p><div id="progressBar"><div id="progressBar_Track"></div></div><p id="progressBarTxt">100/100</p><p>正在更新中，请稍候</p><div class="box"><a href="javascript:;" class="btn layer-btn" id="update_cannel">取消</a></div></div></div>',
//            style: 'width:90%;background-color: transparent;  box-shadow: none;'
//        });
//        //设置最大值
//        ProgressBar.maxValue = 100;
//        //设置当前刻度
//        var index = 0;
//        var mProgressTimer = window.setInterval(function () {
//            index += 2;
//            ProgressBar.SetValue(index);
//            if (index == 100) {
//                window.clearInterval(mProgressTimer);
//                ProgressBar.SetValue(0);
//                layer.closeAll();
//                tips("更新成功!");
//            }
//        }, 100);
//        //取消更新
//        $("#update_cannel").click(function () {
//            window.clearInterval(mProgressTimer);
//            layer.closeAll();
//        })
//    })
//    /*升级 重启  恢复出厂 end*/
//})
//
//var ProgressBar = {
//    maxValue: 1000,
//    value: 0,
//    SetValue: function (aValue) {
//        this.value = aValue;
//        if (this.value >= this.maxValue)
//            this.value = this.maxValue;
//        if (this.value <= 0)
//            this.value = 0;
//        var mWidth = this.value / this.maxValue * $("#progressBar").width() + "px";
//        $("#progressBar_Track").css("width", mWidth);
//        $("#progressBarTxt").html(this.value + "/" + this.maxValue);
//    }
//}

function tips(content) {
    layer.open({
        title: false,
        shadeClose: false,
        content: content,
        style: 'background-color:#000;opacity:.75; box-shadow: none; text-align:center;color:#fff;',
        time: 1
    });
}