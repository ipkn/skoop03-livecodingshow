(function(){
    logHandler = function(x)
    {
        if (x[0] == "chat")
        {
            if (x[1].indexOf("ipkn : ") == 0 || x[1].indexOf(" ") == 0)
            {
                x[1] = '<span style="background-color:yellow">' + x[1] + "</span>";
            }
        }

        return x;
    }
})();
