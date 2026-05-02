function main() {
    var my_canvas = document.getElementById("my_canvas");
    var ctx = my_canvas.getContext("2d");
    ctx.fillStyle = "rgb(200 0 0)";
    ctx.fillRect(10, 10, 50, 50);

    ctx.fillStyle = "rgb(0 0 200 / 50%)";
    ctx.fillRect(30, 30, 50, 50);
}

main();