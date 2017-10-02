/*路由器设置*/
/*窗口拖拽*/
$(document).ready(function() {
    var left, top, $this;
    $(document).delegate('.routerHideCon .routerSetTit', 'mousedown', function(e) {
        left = e.clientX, top = e.clientY, $this = $(this).css('cursor', 'move');
        this.setCapture ? (
            this.setCapture(),
            this.onmousemove = function(ev) {
                mouseMove(ev || event);
            },
            this.onmouseup = mouseUp
        ) : $(document).bind("mousemove", mouseMove).bind("mouseup", mouseUp);
    });

    function mouseMove(e) {
        var target = $this.parents('.routerHideCon');
        var l = Math.max((e.clientX - left + Number(target.css('margin-left').replace(/px$/, '')) || 0), -target.position().left);
        var t = Math.max((e.clientY - top + Number(target.css('margin-top').replace(/px$/, '')) || 0), -target.position().top);
        l = Math.min(l, $(window).width() - target.width() - target.position().left);
        t = Math.min(t, $(window).height() - target.height() - target.position().top);
        left = e.clientX;
        top = e.clientY;
        target.css({
            'margin-left': l,
            'margin-top': t
        });
    }

    function mouseUp(e) {
        var el = $this.get(0);
        el.releaseCapture ?
            (
                el.releaseCapture(),
                el.onmousemove = el.onmouseup = null
            ) : $(document).unbind("mousemove", mouseMove).unbind("mouseup", mouseUp);
    }
})

$(".wifiDevice img").bind("mouseover",wifiDeviceImg1);
$(".wifiDevice img").bind("mouseout",wifiDeviceImg2);
    function wifiDeviceImg1(){
        $(this).css({
            "animation":"myfirst 0.5s",
            "-moz-animation":"myfirst 0.5s", /* Firefox */
            "-webkit-animation":"myfirst 0.5s", /* Safari and Chrome */
            "-o-animation":"myfirst 0.5s" /* Opera */
        })
    }
    function wifiDeviceImg2(){
        $(this).css({
            "animation":"second 0.5s",
            "-moz-animation":"second 0.5s", /* Firefox */
            "-webkit-animation":"second 0.5s", /* Safari and Chrome */
            "-o-animation":"second 0.5s" /* Opera */
        })
    }

$(".routerBg").ready(function(){
    $(".wifiDevice").animate({
        right: '5%',
        top:'23%',
        opacity:1
    },900);
    $(".setLan").animate({
        right: '16%',
        top:'60%',
        opacity:1
    },900);
    $(".reboot").animate({
        left: '39%',
        top:'81%',
        opacity:1
    },900);
    $(".wanDevice").animate({
        left: '11.5%',
        top:'55%',
        opacity:1
    },900);
    $(".interNet").animate({
        left: '11.5%',
        top:'8%',
        opacity:1
    },900);
    $(".changePwd").animate({
        left: '59%',
        top:'11%',
        opacity:1
    },900);
    $(".moblieDevice").animate({
        left: '29%',
        top:'6%',
        opacity:1
    },900);
    $(".wifiName").animate({
        right: '20%',
        top:'33%',
        opacity:1
    },900);
    $(".ipAddr").animate({
        left: '26%',
        top:'63%',
        opacity:1
    },900);
})


   function routerShowTip(th) {
        $(th).next(".hideTipCon").fadeIn();
    }

    function routerHideTip(th) {
        $(th).next(".hideTipCon").fadeOut();
    }