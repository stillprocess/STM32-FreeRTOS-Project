# STM32 + FreeRTOS + OneNET 前端

## 功能

- 显示 `temp_value` 和 `humidity_value`
- 显示并设置四个报警阈值
- 按固件规则判断上下限报警
- 自动刷新、手动刷新、历史曲线和 CSV 导出
- 历史曲线仅记录浏览器打开期间的采样点，刷新页面后清空
- OneNET 密钥只在 Vite 本地服务中使用，不进入浏览器代码

## 配置

1. 将 `.env.example` 复制为 `.env`
2. 填写 OneNET 用户 ID、Access Key、产品 ID 和设备名称
3. 执行 `npm install`
4. 执行 `npm run dev`
5. 浏览器访问终端显示的地址

物模型标识符必须为：

- `temp_value`
- `humidity_value`
- `maxtemp_set`
- `minitemp_set`
- `maxhum_set`
- `minihum_set`

## 构建

执行 `npm run build`。由于 OneNET 鉴权在本地代理中完成，部署到公网时需要把 `/api/device` 和 `/api/thresholds` 放到服务端，不能把 Access Key 写入前端。
