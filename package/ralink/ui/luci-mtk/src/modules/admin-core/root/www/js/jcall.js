
//ui


var jLay;
(function($){
 $(document).ready(function($){
	  jLay=new jLayouts(jQuery);
	  jLay.button({trg:'.btns,.tab'});
	  jLay.tab({trgarea:'.tabs',trg: '.tb',rsparea: '.tbws',rsp: '.tbw',index: 0,lev: 3,ev:'click'});
//		  jLay.size({trg:'.main',mode:'screen',dir:'y',ett:33});
//		  jLay.size({trg:'.uir_wrap,.uir_con,.ui_f_ifr',mode:'screen',sizename:'height'});
//		  jLay.size({trg:'.uir_con .con,.scrollfix',mode:'screen',sizename:'height',ett:64});
//		  jLay.size({trg:'.ui_f_wrap',mode:'screen',sizename:'height',ett:42});
	  jLay.size({trg:'.ht',mode:'parent',dir:'y',rsp:'.ht_w',ett:2});
//	  jLay.scroller({trg:'.scrollwrap',size:8,xBarSize:'auto',yBarSize:'auto',wheelDirection:'y',wheelStepSize:120});
	  jLay.status({trg:'.sbtn',rsp:'.swrps',ev:'click',blur:false});
	  jLay.status({trg:'.sbtns',rsp:'.swrps',ev:'click'});
	  jLay.status({trg:'.sovr',rsp:'.swrps',ev:'hover'});

 });
})(jQuery);

 