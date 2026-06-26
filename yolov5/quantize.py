import argparse
import os
import sys
from pathlib import Path

import torch
import torch.nn as nn
from torch.utils.data import DataLoader, Dataset
# 旧版PyTorch兼容
from torch.quantization import (
    get_default_qconfig,
    prepare,
    convert,
)
from PIL import Image
from utils.augmentations import letterbox
import numpy as np

# -------------------------- YOLOv5 校准数据集 --------------------------
class YOLOv5CalibDataset(Dataset):
    def __init__(self, img_dir, img_size=640):
        self.img_dir = Path(img_dir)
        self.img_size = img_size
        self.img_paths = [f for f in self.img_dir.rglob("*") 
                         if f.suffix.lower() in (".jpg", ".jpeg", ".png", ".bmp")]

    def __len__(self):
        return len(self.img_paths)

    def __getitem__(self, idx):
        img_path = str(self.img_paths[idx])
        img = Image.open(img_path).convert("RGB")
        img = np.array(img)
        
        # YOLOv5官方预处理
        img = letterbox(img, self.img_size, stride=32, auto=False)[0]
        img = img.transpose((2, 0, 1))[::-1]  # HWC->CHW, BGR->RGB
        img = np.ascontiguousarray(img)
        img = torch.from_numpy(img).float()  # 强制FP32
        img /= 255.0
        return img

# -------------------------- 核心修复：YOLOv5模型加载（强制FP32） --------------------------
def load_yolov5_model(model_path, device="cpu"):
    print(f"加载YOLOv5模型：{model_path}", flush=True)
    
    # 1. 加载模型
    model = torch.load(model_path, map_location=device)
    model = model["model"] if isinstance(model, dict) else model
    model.to(device)
    model.eval()

    model.float()
    # print("已将模型强制转换为 FP32 单精度")

    try:
        model.fuse()
        print("BN层融合完成", flush=True)
    except Exception as e:
        print(f"融合BN层警告: {e}", flush=True)

    # 🔥 修复3：禁用半精度/自动混合精度
    for param in model.parameters():
        param.requires_grad = False

    print("YOLOv5模型加载完成（FP32 CPU模式）", flush=True)
    return model

# -------------------------- YOLOv5 静态量化 --------------------------
def yolov5_static_quant(model, calib_loader, output_path, img_size=640):
    device = torch.device("cpu")
    model.to(device)

    # 量化配置
    model.qconfig = get_default_qconfig("fbgemm")
    print("量化后端：fbgemm (CPU INT8)", flush=True)

    # 插入量化节点
    prepare(model, inplace=True)

    # 校准过程
    print("开始校准（请勿中断）...", flush=True)
    with torch.no_grad():
        for i, img in enumerate(calib_loader):
            model(img.to(device))
            print(f"校准进度：{i}/{len(calib_loader)}", flush=True)

    # 转换为量化模型
    convert(model, inplace=True)
    print("YOLOv5 INT8静态量化完成！", flush=True)

    # 保存量化模型
    quant_model_path = os.path.join(output_path, "yolov5_quant_int8.pt")
    torch.save({"model": model}, quant_model_path)
    print(f"量化模型保存至：{quant_model_path}", flush=True)
    return quant_model_path

# -------------------------- 主函数 --------------------------
def main():
    parser = argparse.ArgumentParser(description="YOLOv5 专用INT8量化工具")
    parser.add_argument("--model", required=True, help="YOLOv5模型路径 (best.pt/last.pt)")
    parser.add_argument("--calib_data", required=True, help="校准集路径 (val/images)")
    parser.add_argument("--output", required=True, help="量化模型输出文件夹")
    parser.add_argument("--batch_size", type=int, default=4, help="校准批次大小（建议4/8）")
    parser.add_argument("--img_size", type=int, default=640, help="模型输入尺寸")
    args = parser.parse_args()

    # 路径校验
    if not os.path.exists(args.model):
        raise FileNotFoundError(f"模型不存在：{args.model}")
    if not os.path.exists(args.calib_data):
        raise FileNotFoundError(f"校准集不存在：{args.calib_data}")
    os.makedirs(args.output, exist_ok=True)

    # 1. 加载模型（已强制FP32）
    model = load_yolov5_model(args.model)

    # 2. 加载校准数据
    calib_dataset = YOLOv5CalibDataset(args.calib_data, args.img_size)
    if len(calib_dataset) == 0:
        raise Exception("校准集为空，请检查图片路径！")
    
    calib_loader = DataLoader(
        calib_dataset,
        batch_size=args.batch_size,
        shuffle=False,
        num_workers=0,
        pin_memory=False
    )
    print(f"校准集加载完成：{len(calib_dataset)} 张图片", flush=True)

    # 3. 执行量化
    yolov5_static_quant(model, calib_loader, args.output, args.img_size)

if __name__ == "__main__":
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    try:
        main()
    except Exception as e:
        print(f"量化失败：{str(e)}", flush=True)
        raise