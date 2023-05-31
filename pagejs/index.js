// <script src="https://cdn.jsdelivr.net/npm/vue@2/dist/vue.js"></script>
var app = new Vue({
  el: "#app",
  data: {
    message: "Hello Vue!",
  },
  methods: {
    loadData() {
      console.log("data loaded!");
    },
    onClickDownload() {
      console.log("现在下载！芜芜芜！");
    },
  },
  mounted: function () {
    this.loadData();
  },
});
