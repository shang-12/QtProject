
import warnings
warnings.filterwarnings("ignore")

import argparse
import json

import sys
import os
import cv2
import time
import random
import torch
import numpy as np
import torch.backends.cudnn as cudnn

from models.common import DetectMultiBackend
from utils.augmentations import letterbox
from utils.general import (LOGGER, Profile, check_file, check_img_size, check_imshow, check_requirements, colorstr, cv2,
                           increment_path, non_max_suppression, print_args, scale_boxes, strip_optimizer, xyxy2xywh)
from utils.plots import Annotator, colors, save_one_box, plot_one_box2
from utils.torch_utils import select_device, smart_inference_mode



def main():
    parser = argparse.ArgumentParser()
    # 新增：接收模型路径参数
    parser.add_argument('--model', type=str, required=True, help='模型文件路径')
    parser.add_argument('--img', type=str, required=True, help='图片路径')
    parser.add_argument('--data', type=str, required=True, default='D:/project/qtProject/build/Desktop_Qt_6_9_3_MinGW_64_bit_Debug/temp_dataset.yaml' ,help='数据配置文件')
    parser.add_argument('--conf_thres', type=float, default=0.25, help='置信度阈值')
    parser.add_argument('--iou_thres', type=float, default=0.45, help='iou阈值')
    args = parser.parse_args()

    device = '0'

    imgsz = (320, 320)
    # 加载选择的模型
    device = select_device(device)
    model = DetectMultiBackend(args.model, device=device, data=args.data, dnn=False, fp16 = False, fuse = True)
    stride, names, pt = model.stride, model.names, model.pt
    imgsz = check_img_size(imgsz, s=stride)  # check image size
    colors = [[random.randint(0, 255) for _ in range(3)] for _ in names]

    img0 = cv2.imread(args.img)
    showimg = img0
    with torch.no_grad():
        # 图像预处理
        img = letterbox(img0, new_shape=imgsz)[0]
        img = img[:, :, ::-1].transpose(2, 0, 1)  # BGR to RGB, to 3x416x416
        img = np.ascontiguousarray(img)
        img = torch.from_numpy(img).to(device)
        img = img.float()  # uint8 to fp16/32
        img /= 255.0  # 0 - 255 to 0.0 - 1.0
        if img.ndimension() == 3:
            img = img.unsqueeze(0)
        
        # 检测
        pred = model(img)[0]
        
        # NMS
        pred = non_max_suppression(pred, args.conf_thres, args.iou_thres, classes=0)
        
        info_show = ""
        name_list = []
        res = {"boxes": [], "classes": [], "confidences": []}
        # Process detections
        for i, det in enumerate(pred):
            if det is not None and len(det):
                # Rescale boxes from img_size to im0 size
                det[:, :4] = scale_boxes(img.shape[2:], det[:, :4], showimg.shape).round()
                    
                
                for *xyxy, conf, cls in reversed(det):
                    x1, y1, x2, y2 = xyxy
                    h_img, w_img = showimg.shape[:2]
                    # 归一化坐标
                    cx = (x1 + x2) / 2 / w_img
                    cy = (y1 + y2) / 2 / h_img
                    w = (x2 - x1) / w_img
                    h = (y2 - y1) / h_img
                    res["boxes"].append({"cx": float(cx), "cy": float(cy), "w": float(w), "h": float(h)})
                    res["classes"].append(str(names[int(cls)]))
                    res["confidences"].append(conf.item())
                    label = '%s %.2f' % (names[int(cls)], conf)
                    name_list.append(names[int(cls)])
                    single_info = plot_one_box2(xyxy, showimg, label=label, color=colors[int(cls)], line_thickness=2)

        
    # 输出JSON给Qt解析


    model_name = args.model.split('/')[-1].strip('.pt')
    img_name = args.img.split('/')[-1]
    img_save_path = 'D:/project/qtProject/build/Desktop_Qt_6_9_3_MinGW_64_bit_Debug/runs/bes'
    if not os.path.exists(img_save_path):
        os.makedirs(img_save_path)
    cv2.imwrite(img_save_path + '/' + img_name, showimg)
    res["result_img_path"] = f"{img_save_path}/{img_name}"
    print("图像保存至：", img_save_path + '/' + img_name, flush=True)
    print(json.dumps(res, ensure_ascii=False), flush=True)

if __name__ == "__main__":
    main()