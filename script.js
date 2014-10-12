(function(){
    var visible = false;
    function showSquare()
    {
        if (visible)
            return;
        visible = true;
        var div = document.createElement("div");
        div.style.top = "30%";
        div.style.left = "30%";
        div.style.width = "40%";
        div.style.height = "40%";
        div.style.backgroundColor = "red";
        div.style.position = "absolute";
        div.id = "red";
        $("body").append(div);
    }

    function hideSquare()
    {
        visible = false;
        $("#red").remove()
    }

    logHandler = function(x)
    {
        if (x[0] == "chat")
        {
            if (x[1] == "ipkn : /show")
            {
                showSquare();
            }
            else if (x[1] == "ipkn : /hide")
            {
                hideSquare();
            }
            if (x[1].indexOf("ipkn : ") == 0 || x[1].indexOf(" ") == 0)

            {
                x[1] = '<span style="background-color:yellow">' + x[1] + "</span>";
            }
        }

        return x;
    }
})();
