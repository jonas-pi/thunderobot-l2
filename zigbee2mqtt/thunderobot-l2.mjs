import * as m from "zigbee-herdsman-converters/lib/modernExtend";

export default {
    zigbeeModel: ["L2"],
    model: "L2",
    vendor: "Thunderobot",
    description: "Thunderobot L2 screen light",
    extend: [
        m.identify(),
        m.light({colorTemp: {range: [153, 333]}}),
    ],
};
