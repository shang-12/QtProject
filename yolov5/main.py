import sys
import os
import cv2
import time
import random
import torch
import numpy as np
import torch.backends.cudnn as cudnn

from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *

from models.common import DetectMultiBackend
from utils.augmentations import letterbox
from utils.general import (LOGGER, Profile, check_file, check_img_size, check_imshow, check_requirements, colorstr, cv2,
                           increment_path, non_max_suppression, print_args, scale_boxes, strip_optimizer, xyxy2xywh)
from utils.plots import Annotator, colors, save_one_box, plot_one_box2
from utils.torch_utils import select_device, smart_inference_mode

import ui.detect as UI_Layer

class Logic_Layer(QtWidgets.QMainWindow):
    def __init__(self, parent = None):
        super(Logic_Layer, self).__init__(parent)
        
        self.timer_video = QtCore.QTimer()
        self.ui = UI_Layer.Ui_MainWindow()
        self.ui.setupUi(self)
        self.setupSlots()
        
        self.img_path = ""
        self.model_path = str(os.path.dirname(os.path.abspath(__file__))).replace('\\', '/') + '/weights/modify.pt'
        self.ui.lineEdit_model.setText(self.model_path)
        
        self.device = '0'
        self.imgsz = (640, 640)
        self.half = False
        self.save_path = 'output/'
        self.conf_thres = 0.25
        self.iou_thres = 0.45
        self.classes = 0
        self.load_model()

        self.camera = None
        self.vid_writer = None

    def setupSlots(self):
        self.ui.pushButton_img.clicked.connect(self.img_open)
        self.ui.pushButton_model.clicked.connect(self.model_select)
        self.ui.pushButton_camera.clicked.connect(self.camera_open)
        self.ui.pushButton_finish.clicked.connect(self.finish_detect)
        self.ui.pushButton_exit.clicked.connect(self.exit)
        
        self.timer_video.timeout.connect(self.show_camera_frame)

    def detect(self, name_list, img):
        showimg = img
        with torch.no_grad():
            # 图像预处理
            img = letterbox(img, new_shape=self.imgsz)[0]
            img = img[:, :, ::-1].transpose(2, 0, 1)  # BGR to RGB, to 3x416x416
            img = np.ascontiguousarray(img)
            img = torch.from_numpy(img).to(self.device)
            img = img.half() if self.half else img.float()  # uint8 to fp16/32
            img /= 255.0  # 0 - 255 to 0.0 - 1.0
            if img.ndimension() == 3:
                img = img.unsqueeze(0)
            
            # 检测
            pred = self.model(img)[0]
            
            # NMS
            pred = non_max_suppression(pred, self.conf_thres, self.iou_thres, classes=self.classes)
            
            info_show = ""
            # Process detections
            for i, det in enumerate(pred):
                if det is not None and len(det):
                    # Rescale boxes from img_size to im0 size
                    det[:, :4] = scale_boxes(img.shape[2:], det[:, :4], showimg.shape).round()
                    for c in det[:, 5].unique():
                        if self.names[int(c)] == 'person':
                            n = (det[:, 5] == c).sum()  # detections per class
                            info_show += f"当前共有{n}个人\n"  # add to string
                        else:
                            continue
                    
                    for *xyxy, conf, cls in reversed(det):
                        label = '%s %.2f' % (self.names[int(cls)], conf)
                        name_list.append(self.names[int(cls)])
                        single_info = plot_one_box2(xyxy, showimg, label=label, color=self.colors[int(cls)], line_thickness=2)
                        info_show += single_info + "\n"
        return info_show

    def img_open(self):
        name_list = []
        self.img_path = str(QFileDialog.getOpenFileName(self.ui.pushButton_img, "打开图片", "images", "*jpg;;*png;;All Files(*)")[0])
        if self.img_path == '':
            QtWidgets.QMessageBox.warning(self, 'Warning', '打开图片失败', QtWidgets.QMessageBox.Ok, QtWidgets.QMessageBox.Ok)
        else:
            self.ui.lineEdit_img.setText(self.img_path)
            img = cv2.imread(self.img_path)
            #yolo.run(weights = self.model_path, source = self.img_path, save_txt = True, classes = 0, project = "output", name = self.model_path.split('/')[-1].strip(".pt"), exist_ok = True)
            info = self.detect(name_list, img)
            self.ui.textBrowser_info.setText(info)
            
            model_name = self.model_path.split('/')[-1].strip('.pt')
            img_name = self.img_path.split('/')[-1]
            img_save_path = '' + self.save_path + model_name
            if not os.path.exists(img_save_path):
                os.makedirs(img_save_path)
            cv2.imwrite(img_save_path + '/' + img_name, img)
            
            img = cv2.cvtColor(img, cv2.COLOR_BGR2BGRA)
            img = cv2.resize(img, (320, 320), interpolation = cv2.INTER_AREA)
            qtImg = QtGui.QImage(img.data, img.shape[1], img.shape[0], QtGui.QImage.Format_RGB32)
            
            self.ui.label_result.setText("")
            self.ui.label_result.setPixmap(QtGui.QPixmap.fromImage(qtImg))
            self.ui.label_result.setScaledContents(True)
    
    def load_model(self):
            self.device = select_device(self.device)
            cudnn.benchmark = True
            self.model = DetectMultiBackend(self.model_path, device=self.device, data='yolov5/data/VOC.yaml', dnn=False, fp16 = False, fuse = True)
            self.stride, self.names, self.pt = self.model.stride, self.model.names, self.model.pt
            self.imgsz = check_img_size(self.imgsz, s=self.stride)  # check image size
            self.colors = [[random.randint(0, 255) for _ in range(3)] for _ in self.names]
            QtWidgets.QMessageBox.information(self, "Notice", "模型加载完成", QtWidgets.QMessageBox.Ok, QtWidgets.QMessageBox.Ok)
        
    def model_select(self):
        self.model_path = str(QFileDialog.getOpenFileName(self.ui.pushButton_model, '选择模型', 'weights/', '*pt')[0])
        if self.model_path == '':
            QtWidgets.QMessageBox.warning(self, 'Warning', '打开权重失败', QtWidgets.QMessageBox.Ok, QtWidgets.QMessageBox.Ok)
        else:
            self.ui.lineEdit_model.setText(self.model_path)
            self.ui.lineEdit_img.clear()
            self.ui.label_result.clear()
            self.ui.label_result.setText("选择图片或打开摄像头进行检测")
            self.ui.textBrowser_info.clear()
            self.img_path = ""
            self.load_model()

    def show_camera_frame(self):
        name_list = []
        _, img = self.camera.read()
        if img is not None:
            info_show = self.detect(name_list, img)
            self.vid_writer.write(img)
            
            self.ui.textBrowser_info.setText(info_show)

            show = cv2.resize(img, (640, 480))
            self.result = cv2.cvtColor(show, cv2.COLOR_BGR2RGB)
            showImage = QtGui.QImage(self.result.data, self.result.shape[1], self.result.shape[0],
                                     QtGui.QImage.Format_RGB888)
            self.ui.label_result.setPixmap(QtGui.QPixmap.fromImage(showImage))
            self.ui.label_result.setScaledContents(True)

        else:
            self.finish_detect()

    def camera_open(self):
        self.ui.lineEdit_img.clear()
        self.camera_num = 0
        self.camera = cv2.VideoCapture(self.camera_num)
        camera_on = self.camera.isOpened()
        if not camera_on:
            QtWidgets.QMessageBox.warning(self, "Warning", "摄像头未打开", QtWidgets.QMessageBox.Ok, QtWidgets.QMessageBox.Ok)
        else:
            fps = self.camera.get(cv2.CAP_PROP_FPS)
            w = int(self.camera.get(cv2.CAP_PROP_FRAME_WIDTH))
            h = int(self.camera.get(cv2.CAP_PROP_FRAME_HEIGHT))
            
            model_name = self.model_path.split('/')[-1].strip('.pt')
            now = time.strftime("%Y-%m-%d-%H-%M-%S", time.localtime(time.time()))
            camera_save_path = self.save_path + model_name
            if not os.path.exists(camera_save_path):
                os.makedirs(camera_save_path)
            self.vid_writer = cv2.VideoWriter(camera_save_path + '/' + now + '.mp4', cv2.VideoWriter_fourcc(*'mp4v'), fps, (w, h))
            
            self.timer_video.start(30)
            
            self.ui.pushButton_img.setDisabled(True)
            self.ui.pushButton_camera.setDisabled(True)

    def finish_detect(self):
        if self.camera is not None:
            self.camera.release()
        if self.vid_writer is not None:
            self.vid_writer.release()
        self.ui.label_result.clear()
        self.ui.label_result.setText("选择图片或打开摄像头进行检测")
        self.ui.textBrowser_info.clear()
        self.ui.lineEdit_img.clear()
        self.ui.pushButton_camera.setDisabled(False)
        self.ui.pushButton_img.setDisabled(False)

    def exit(self):
        sys.exit(0)

if __name__ == '__main__':
    app = QtWidgets.QApplication(sys.argv)
    win = Logic_Layer()
    win.show()
    sys.exit(app.exec_())